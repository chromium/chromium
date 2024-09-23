// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/unique_name/unique_name_helper.h"

#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page_state/page_state_serialization.h"

namespace blink {
namespace {

// Requested names longer than this (that are unique) should be hashed.
constexpr size_t kMaxSize = 80;

class TestFrameAdapter : public UniqueNameHelper::FrameAdapter {
 public:
  // |virtual_index_in_parent| is the virtual index of this frame in the
  // parent's list of children, as unique name generation should see it. Note
  // that this may differ from the actual index of this adapter in
  // |parent_->children_|.
  explicit TestFrameAdapter(TestFrameAdapter* parent,
                            int virtual_index_in_parent,
                            const std::string& requested_name)
      : parent_(parent),
        virtual_index_in_parent_(virtual_index_in_parent),
        unique_name_helper_(this) {
    if (parent_)
      parent_->children_.push_back(this);
    unique_name_helper_.UpdateName(requested_name);
    CalculateLegacyName(requested_name);
  }

  ~TestFrameAdapter() override {
    if (parent_) {
      parent_->children_.erase(base::ranges::find(parent_->children_, this));
    }
  }

  bool IsMainFrame() const override { return !parent_; }

  bool IsCandidateUnique(std::string_view name) const override {
    auto* top = this;
    while (top->parent_)
      top = top->parent_;
    return top->CheckUniqueness(name);
  }

  int GetSiblingCount() const override { return virtual_index_in_parent_; }

  int GetChildCount() const override {
    ADD_FAILURE()
        << "GetChildCount() should not be triggered by unit test code!";
    return 0;
  }

  std::vector<std::string> CollectAncestorNames(
      BeginPoint begin_point,
      bool (*should_stop)(std::string_view)) const override {
    EXPECT_EQ(BeginPoint::kParentFrame, begin_point);
    std::vector<std::string> result;
    for (auto* adapter = parent_.get(); adapter; adapter = adapter->parent_) {
      result.push_back(adapter->GetNameForCurrentMode());
      if (should_stop(result.back()))
        break;
    }
    return result;
  }

  std::vector<int> GetFramePosition(BeginPoint begin_point) const override {
    EXPECT_EQ(BeginPoint::kParentFrame, begin_point);
    std::vector<int> result;
    for (auto* adapter = this; adapter->parent_; adapter = adapter->parent_)
      result.push_back(adapter->virtual_index_in_parent_);
    return result;
  }

  // Returns the new style name with a max size limit.
  const std::string& GetUniqueName() const {
    return unique_name_helper_.value();
  }

  // Calculate and return the legacy style name with no max size limit.
  const std::string& GetLegacyName() const { return legacy_name_; }

  // Populate a tree of FrameState with legacy unique names. The order of
  // FrameState children is guaranteed to match the order of TestFrameAdapter
  // children.
  void PopulateLegacyFrameState(ExplodedFrameState* frame_state) const {
    frame_state->target = base::UTF8ToUTF16(GetLegacyName());
    frame_state->children.resize(children_.size());
    for (size_t i = 0; i < children_.size(); ++i)
      children_[i]->PopulateLegacyFrameState(&frame_state->children[i]);
  }

  // Recursively verify that FrameState and its children have matching unique
  // names to this TestFrameAdapter.
  void VerifyUpdatedFrameState(const ExplodedFrameState& frame_state) const {
    EXPECT_EQ(GetUniqueName(),
              base::UTF16ToUTF8(frame_state.target.value_or(std::u16string())));

    ASSERT_EQ(children_.size(), frame_state.children.size());
    for (size_t i = 0; i < children_.size(); ++i) {
      children_[i]->VerifyUpdatedFrameState(frame_state.children[i]);
    }
  }

  void UpdateName(const std::string& new_name) {
    unique_name_helper_.UpdateName(new_name);
  }

  void Freeze() { unique_name_helper_.Freeze(); }

 private:
  // Global toggle for the style of name to generate. Used to ensure that test
  // code can consistently trigger the legacy generation path when needed.
  static bool generate_legacy_name_;

  const std::string& GetNameForCurrentMode() const {
    return generate_legacy_name_ ? GetLegacyName() : GetUniqueName();
  }

  void CalculateLegacyName(const std::string& requested_name) {
    // Manually skip the main frame so its legacy name is always empty: this
    // is needed in the test as that logic lives at a different layer in
    // UniqueNameHelper.
    if (!IsMainFrame()) {
      base::AutoReset<bool> enable_legacy_mode(&generate_legacy_name_, true);
      legacy_name_ =
          UniqueNameHelper::CalculateLegacyNameForTesting(this, requested_name);
    }
  }

  bool CheckUniqueness(std::string_view name) const {
    if (name == GetNameForCurrentMode())
      return false;
    for (TestFrameAdapter* child : children_) {
      if (!child->CheckUniqueness(name))
        return false;
    }
    return true;
  }

  const raw_ptr<TestFrameAdapter> parent_;
  std::vector<raw_ptr<TestFrameAdapter, VectorExperimental>> children_;
  const int virtual_index_in_parent_;
  UniqueNameHelper unique_name_helper_;
  std::string legacy_name_;
};

bool TestFrameAdapter::generate_legacy_name_ = false;

// Test helper that verifies that legacy unique names in versions of PageState
// prior to 25 are correctly updated when deserialized.
void VerifyPageStateForTargetUpdate(const TestFrameAdapter& main_frame) {
  ExplodedPageState in_state;
  main_frame.PopulateLegacyFrameState(&in_state.top);

  // Version 24 is the last version with unlimited size unique names.
  std::string encoded_state;
  LegacyEncodePageStateForTesting(in_state, 24, &encoded_state);

  ExplodedPageState out_state;
  DecodePageState(encoded_state, &out_state);

  main_frame.VerifyUpdatedFrameState(out_state.top);
}

TEST(UniqueNameHelper, Basic) {
  // Main frames should always have an empty unique name.
  TestFrameAdapter main_frame(nullptr, -1, "my main frame");
  EXPECT_EQ("", main_frame.GetUniqueName());
  EXPECT_EQ("", main_frame.GetLegacyName());

  // A child frame with a requested name that is unique should use the requested
  // name.
  TestFrameAdapter frame_0(&main_frame, 0, "child frame with name");
  EXPECT_EQ("child frame with name", frame_0.GetUniqueName());
  EXPECT_EQ("child frame with name", frame_0.GetLegacyName());

  // A child frame with no requested name should receive a generated unique
  // name.
  TestFrameAdapter frame_7(&main_frame, 7, "");
  EXPECT_EQ("<!--framePath //<!--frame7-->-->", frame_7.GetUniqueName());
  EXPECT_EQ("<!--framePath //<!--frame7-->-->", frame_7.GetLegacyName());

  // Naming collision should force a fallback to using a generated unique name.
  TestFrameAdapter frame_2(&main_frame, 2, "child frame with name");
  EXPECT_EQ("<!--framePath //<!--frame2-->-->", frame_2.GetUniqueName());
  EXPECT_EQ("<!--framePath //<!--frame2-->-->", frame_2.GetLegacyName());

  // Index collision should also force a fallback to using a generated unique
  // name.
  TestFrameAdapter frame_2a(&main_frame, 2, "");
  EXPECT_EQ("<!--framePath //<!--frame2-->--><!--framePosition-2/0-->",
            frame_2a.GetUniqueName());
  EXPECT_EQ("<!--framePath //<!--frame2-->--><!--framePosition-2/0-->",
            frame_2a.GetLegacyName());

  // A child of a frame with a unique naming collision will incorporate the
  // frame position marker as part of its frame path, though it will look a bit
  // strange...
  TestFrameAdapter frame_2a_5(&frame_2a, 5, "");
  EXPECT_EQ(
      "<!--framePath //<!--frame2-->--><!--framePosition-2/0/<!--frame5-->-->",
      frame_2a_5.GetUniqueName());
  EXPECT_EQ(
      "<!--framePath //<!--frame2-->--><!--framePosition-2/0/<!--frame5-->-->",
      frame_2a_5.GetLegacyName());

  // Index and name collision should also force a fallback to using a generated
  // unique name.
  TestFrameAdapter frame_2b(&main_frame, 2, "child frame with name");
  EXPECT_EQ("<!--framePath //<!--frame2-->--><!--framePosition-2/1-->",
            frame_2b.GetUniqueName());
  EXPECT_EQ("<!--framePath //<!--frame2-->--><!--framePosition-2/1-->",
            frame_2b.GetLegacyName());

  VerifyPageStateForTargetUpdate(main_frame);
}

TEST(UniqueNameHelper, Hashing) {
  // Main frames should always have an empty unique name.
  TestFrameAdapter main_frame(nullptr, -1, "my main frame");
  EXPECT_EQ("", main_frame.GetUniqueName());
  EXPECT_EQ("", main_frame.GetLegacyName());

  // A child frame with a requested name that is unique but too long should fall
  // back to hashing.
  const std::string too_long_name(kMaxSize + 1, 'a');
  TestFrameAdapter frame_0(&main_frame, 0, too_long_name);
  EXPECT_EQ(
      "<!--"
      "frameHash8C48280D57FB88F161ADF34D9F597D93CA32B7EDFCD23B2AFE64C3789B3F785"
      "5-->",
      frame_0.GetUniqueName());
  EXPECT_EQ(too_long_name, frame_0.GetLegacyName());

  // A child frame with no requested name should receive a generated unique
  // name.
  TestFrameAdapter frame_7(&main_frame, 7, "");
  EXPECT_EQ("<!--framePath //<!--frame7-->-->", frame_7.GetUniqueName());
  EXPECT_EQ("<!--framePath //<!--frame7-->-->", frame_7.GetLegacyName());

  // Verify that a requested name that's over the limit collides with the hashed
  // version of its requested name.
  TestFrameAdapter frame_2(&main_frame, 2, too_long_name);
  EXPECT_EQ("<!--framePath //<!--frame2-->-->", frame_2.GetUniqueName());
  EXPECT_EQ("<!--framePath //<!--frame2-->-->", frame_2.GetLegacyName());

  // Index collision should also force a fallback to using a generated unique
  // name.
  TestFrameAdapter frame_2a(&main_frame, 2, "");
  EXPECT_EQ("<!--framePath //<!--frame2-->--><!--framePosition-2/0-->",
            frame_2a.GetUniqueName());
  EXPECT_EQ("<!--framePath //<!--frame2-->--><!--framePosition-2/0-->",
            frame_2a.GetLegacyName());

  // A child of a frame with a unique naming collision will incorporate the
  // frame position marker as part of its frame path, though it will look a bit
  // strange...
  TestFrameAdapter frame_2a_5(&frame_2a, 5, "");
  EXPECT_EQ(
      "<!--framePath //<!--frame2-->--><!--framePosition-2/0/<!--frame5-->-->",
      frame_2a_5.GetUniqueName());
  EXPECT_EQ(
      "<!--framePath //<!--frame2-->--><!--framePosition-2/0/<!--frame5-->-->",
      frame_2a_5.GetLegacyName());

  // Index and name collision should also force a fallback to using a generated
  // unique name.
  TestFrameAdapter frame_2b(&main_frame, 2, too_long_name);
  EXPECT_EQ("<!--framePath //<!--frame2-->--><!--framePosition-2/1-->",
            frame_2b.GetUniqueName());
  EXPECT_EQ("<!--framePath //<!--frame2-->--><!--framePosition-2/1-->",
            frame_2b.GetLegacyName());

  VerifyPageStateForTargetUpdate(main_frame);
}

// Verify that basic frame path generation always includes the full path from
// the root.
TEST(UniqueNameHelper, BasicGeneratedFramePath) {
  TestFrameAdapter main_frame(nullptr, -1, "my main frame");
  EXPECT_EQ("", main_frame.GetUniqueName());
  EXPECT_EQ("", main_frame.GetLegacyName());

  TestFrameAdapter frame_2(&main_frame, 2, "");
  EXPECT_EQ("<!--framePath //<!--frame2-->-->", frame_2.GetUniqueName());
  EXPECT_EQ("<!--framePath //<!--frame2-->-->", frame_2.GetLegacyName());

  TestFrameAdapter frame_2_3(&frame_2, 3, "named grandchild");
  EXPECT_EQ("named grandchild", frame_2_3.GetUniqueName());
  EXPECT_EQ("named grandchild", frame_2_3.GetLegacyName());

  // Even though the parent frame has a unique name, the frame path should
  // include the full path from the root.
  TestFrameAdapter frame_2_3_5(&frame_2_3, 5, "");
  EXPECT_EQ("<!--framePath //<!--frame2-->/named grandchild/<!--frame5-->-->",
            frame_2_3_5.GetUniqueName());
  EXPECT_EQ("<!--framePath //<!--frame2-->/named grandchild/<!--frame5-->-->",
            frame_2_3_5.GetLegacyName());

  VerifyPageStateForTargetUpdate(main_frame);
}

TEST(UniqueNameHelper, GeneratedFramePathHashing) {
  TestFrameAdapter main_frame(nullptr, -1, "my main frame");
  EXPECT_EQ("", main_frame.GetUniqueName());
  EXPECT_EQ("", main_frame.GetLegacyName());

  TestFrameAdapter frame_0(&main_frame, 0, "");
  EXPECT_EQ("<!--framePath //<!--frame0-->-->", frame_0.GetUniqueName());
  EXPECT_EQ("<!--framePath //<!--frame0-->-->", frame_0.GetLegacyName());

  // At the limit, so the hashing fallback should not be triggered.
  const std::string just_fits_name(kMaxSize, 'a');
  TestFrameAdapter frame_0_0(&frame_0, 0, just_fits_name);
  EXPECT_EQ(just_fits_name, frame_0_0.GetUniqueName());
  EXPECT_EQ(just_fits_name, frame_0_0.GetLegacyName());

  // But anything over should trigger hashing.
  const std::string too_long_name(kMaxSize + 1, 'a');
  TestFrameAdapter frame_0_1(&frame_0, 1, too_long_name);
  EXPECT_EQ(
      "<!--"
      "frameHash8C48280D57FB88F161ADF34D9F597D93CA32B7EDFCD23B2AFE64C3789B3F785"
      "5-->",
      frame_0_1.GetUniqueName());
  EXPECT_EQ(too_long_name, frame_0_1.GetLegacyName());

  // A child frame should incorporate the parent's hashed requested name into
  // its frame path.
  TestFrameAdapter frame_0_1_0(&frame_0_1, 0, "");
  EXPECT_EQ(
      "<!--framePath "
      "//<!--frame0-->/"
      "<!--"
      "frameHash8C48280D57FB88F161ADF34D9F597D93CA32B7EDFCD23B2AFE64C3789B3F785"
      "5-->/<!--frame0-->-->",
      frame_0_1_0.GetUniqueName());
  EXPECT_EQ(
      "<!--framePath "
      "//<!--frame0-->/" +
          too_long_name + "/<!--frame0-->-->",
      frame_0_1_0.GetLegacyName());

  // Make sure that name replacement during legacy name updates don't
  // accidentally match on substrings: the name here is intentionally chosen so
  // that too_long_name is a substring.
  const std::string too_long_name2(kMaxSize + 10, 'a');
  TestFrameAdapter frame_0_2(&frame_0, 2, too_long_name2);
  EXPECT_EQ(
      "<!--"
      "frameHash6B2EC79170F50EA57B886DC81A2CF78721C651A002C8365A524019A7ED5A8A4"
      "0-->",
      frame_0_2.GetUniqueName());
  EXPECT_EQ(too_long_name2, frame_0_2.GetLegacyName());

  // Make sure that legacy name updates correctly handle multiple replacements.
  // An unnamed frame is used as the deepest descendant to ensure the requested
  // names from ancestors appear in the frame path. Begin with a named
  // grandparent:
  const std::string too_long_name3(kMaxSize * 2, 'b');
  TestFrameAdapter frame_0_1_1(&frame_0_1, 1, too_long_name3);
  EXPECT_EQ(
      "<!--"
      "frameHash3A0B065A4255F95EF6E206B11004B8805FB631A68F468A72CE26F7592C88C27"
      "A-->",
      frame_0_1_1.GetUniqueName());
  EXPECT_EQ(too_long_name3, frame_0_1_1.GetLegacyName());

  // And a named parent:
  const std::string too_long_name4(kMaxSize * 3, 'c');
  TestFrameAdapter frame_0_1_1_0(&frame_0_1_1, 0, too_long_name4);
  EXPECT_EQ(
      "<!--"
      "frameHashE00D028A784E645656638F4D461B81E779E5225CA9824C8E09664956CF4DAE3"
      "1-->",
      frame_0_1_1_0.GetUniqueName());
  EXPECT_EQ(too_long_name4, frame_0_1_1_0.GetLegacyName());

  // And finally an unnamed child to trigger fallback to the frame path:
  TestFrameAdapter frame_0_1_1_0_0(&frame_0_1_1_0, 0, "");
  EXPECT_EQ(
      "<!--framePath "
      "//<!--frame0-->/"
      "<!--"
      "frameHash8C48280D57FB88F161ADF34D9F597D93CA32B7EDFCD23B2AFE64C3789B3F785"
      "5-->/"
      "<!--"
      "frameHash3A0B065A4255F95EF6E206B11004B8805FB631A68F468A72CE26F7592C88C27"
      "A-->/"
      "<!--"
      "frameHashE00D028A784E645656638F4D461B81E779E5225CA9824C8E09664956CF4DAE3"
      "1-->/<!--frame0-->-->",
      frame_0_1_1_0_0.GetUniqueName());
  EXPECT_EQ("<!--framePath //<!--frame0-->/" + too_long_name + "/" +
                too_long_name3 + "/" + too_long_name4 + "/<!--frame0-->-->",
            frame_0_1_1_0_0.GetLegacyName());

  VerifyPageStateForTargetUpdate(main_frame);
}

TEST(UniqueNameHelper, UpdateName) {
  TestFrameAdapter main_frame(nullptr, -1, "my main frame");
  EXPECT_EQ("", main_frame.GetUniqueName());

  TestFrameAdapter frame_0(&main_frame, 0, "name1");
  EXPECT_EQ("name1", frame_0.GetUniqueName());

  frame_0.UpdateName("name2");
  EXPECT_EQ("name2", frame_0.GetUniqueName());

  frame_0.Freeze();
  frame_0.UpdateName("name3");
  EXPECT_EQ("name2", frame_0.GetUniqueName());  // No change expected.
}

}  // namespace
}  // namespace blink
