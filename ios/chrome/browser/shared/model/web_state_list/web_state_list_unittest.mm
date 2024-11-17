// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

#import "base/memory/raw_ptr.h"
#import "base/scoped_multi_source_observation.h"
#import "base/scoped_observation.h"
#import "base/supports_user_data.h"
#import "components/tab_groups/tab_group_color.h"
#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using tab_groups::TabGroupId;
using tab_groups::TabGroupVisualData;

namespace {
const char kURL0[] = "https://chromium.org/0";
const char kURL1[] = "https://chromium.org/1";
const char kURL2[] = "https://chromium.org/2";
const char kURL3[] = "https://chromium.org/3";
const char kURL4[] = "https://chromium.org/4";
const char kURL5[] = "https://chromium.org/5";
const char kURL6[] = "https://chromium.org/6";

// WebStateList observer that records which events have been called by the
// WebStateList.
class WebStateListTestObserver : public WebStateListObserver {
 public:
  WebStateListTestObserver() = default;

  WebStateListTestObserver(const WebStateListTestObserver&) = delete;
  WebStateListTestObserver& operator=(const WebStateListTestObserver&) = delete;

  void Observe(WebStateList* web_state_list) {
    observation_.AddObservation(web_state_list);
  }

  // Reset statistics whether events have been called.
  void ResetStatistics() {
    web_state_inserted_count_ = 0;
    web_state_inserted_group_ = nullptr;
    web_state_moved_count_ = 0;
    web_state_moved_old_group_ = nullptr;
    web_state_moved_new_group_ = nullptr;
    web_state_replaced_count_ = 0;
    web_state_detached_count_ = 0;
    web_state_activated_count_ = 0;
    pinned_state_changed_count_ = 0;
    status_only_count_ = 0;
    status_only_web_state_ = nullptr;
    status_only_old_group_ = nullptr;
    status_only_new_group_ = nullptr;
    group_created_count_ = 0;
    group_created_group_ = nullptr;
    visual_data_updated_count_ = 0;
    visual_data_updated_group_ = nullptr;
    old_visual_data_ = TabGroupVisualData();
    group_moved_count_ = 0;
    group_moved_group_ = nullptr;
    group_moved_from_range_ = TabGroupRange::InvalidRange();
    group_moved_to_range_ = TabGroupRange::InvalidRange();
    group_deleted_count_ = 0;
    group_deleted_group_ = nullptr;
    batch_operation_started_count_ = 0;
    batch_operation_ended_count_ = 0;
    web_state_list_destroyed_count_ = 0;
  }

  // Returns whether the insertion operation was invoked.
  bool web_state_inserted() const { return web_state_inserted_count_ != 0; }

  // Returns the number of insertion operations.
  int web_state_inserted_count() const { return web_state_inserted_count_; }

  // Returns the destination group for the last inserted WebState.
  const TabGroup* web_state_inserted_group() const {
    return web_state_inserted_group_;
  }

  // Returns whether the move operation was invoked.
  bool web_state_moved() const { return web_state_moved_count_ != 0; }

  // Returns the number of move operations.
  int web_state_moved_count() const { return web_state_moved_count_; }

  // Returns the old group mentioned in a WebStateListChangeMove.
  const TabGroup* web_state_moved_old_group() const {
    return web_state_moved_old_group_;
  }

  // Returns the new group mentioned in a WebStateListChangeMove.
  const TabGroup* web_state_moved_new_group() const {
    return web_state_moved_new_group_;
  }

  // Returns whether the replacement operation was invoked.
  bool web_state_replaced() const { return web_state_replaced_count_ != 0; }

  // Returns the number of replacement operations.
  int web_state_replaced_count() const { return web_state_replaced_count_; }

  // Returns whether a WebState was detached.
  bool web_state_detached() const { return web_state_detached_count_ != 0; }

  // Returns the number of WebState detached.
  int web_state_detached_count() const { return web_state_detached_count_; }

  // Returns the last group for the last detached WebState.
  const TabGroup* web_state_detached_group() const {
    return web_state_detached_group_;
  }

  // Returns whether a WebState was activated.
  bool web_state_activated() const { return web_state_activated_count_ != 0; }

  // Returns the number of WebState activation.
  int web_state_activated_count() const { return web_state_activated_count_; }

  // Returns whether the pinned state was updated.
  bool pinned_state_changed() const { return pinned_state_changed_count_ != 0; }

  // Returns the number of WebState pin changes.
  int pinned_state_changed_count() const { return pinned_state_changed_count_; }

  // Returns the number of status only changes.
  int status_only_count() const { return status_only_count_; }

  // Returns the web state mentioned in a WebStateListChangeStatusOnly.
  web::WebState* status_only_web_state() const {
    return status_only_web_state_;
  }

  // Returns the old group mentioned in a WebStateListChangeStatusOnly.
  const TabGroup* status_only_old_group() const {
    return status_only_old_group_;
  }

  // Returns the new group mentioned in a WebStateListChangeStatusOnly.
  const TabGroup* status_only_new_group() const {
    return status_only_new_group_;
  }

  // Returns the number of groups created.
  int group_created_count() const { return group_created_count_; }

  // Returns a group was moved.
  bool group_created() const { return group_created_count_ != 0; }

  // Returns the group that was created.
  const TabGroup* group_created_group() const { return group_created_group_; }

  // Returns the number of groups visual data updates.
  int visual_data_updated_count() const { return visual_data_updated_count_; }

  // Returns a group had visual data updated.
  bool visual_data_updated() const { return visual_data_updated_count_ != 0; }

  // Returns the group that whose visual data were updated.
  const TabGroup* visual_data_updated_group() const {
    return visual_data_updated_group_;
  }

  // Returns the previous visual data of a group.
  const TabGroupVisualData old_visual_data() const { return old_visual_data_; }

  // Returns the number of groups moved.
  int group_moved_count() const { return group_moved_count_; }

  // Returns a group was moved.
  bool group_moved() const { return group_moved_count_ != 0; }

  // Returns the group that moved.
  const TabGroup* group_moved_group() const { return group_moved_group_; }

  // Returns the previous range of the group that moved.
  TabGroupRange group_moved_from_range() const {
    return group_moved_from_range_;
  }

  // Returns the current range of the group that moved.
  TabGroupRange group_moved_to_range() const { return group_moved_to_range_; }

  // Returns the number of groups deleted.
  int group_deleted_count() const { return group_deleted_count_; }

  // Returns a group was moved.
  bool group_deleted() const { return group_deleted_count_ != 0; }

  // Returns the group that was deleted.
  //
  // In tests, it gives the opportunity to compare pointer addresses even after
  // deletion,but shouldn't be done in real code.
  // Don't use it to pass to WebStateList APIs afterwards.
  const TabGroup* group_deleted_group() const { return group_deleted_group_; }

  // Returns whether WillBeginBatchOperation was invoked.
  bool batch_operation_started() const {
    return batch_operation_started_count_ != 0;
  }

  // Returns the number of times WillBeginBatchOperation was invoked.
  int batch_operation_started_count() const {
    return batch_operation_started_count_;
  }

  // Returns whether BatchOperationEnded was invoked.
  bool batch_operation_ended() const {
    return batch_operation_ended_count_ != 0;
  }

  // Returns the number of times BatchOperationEnded was invoked.
  int batch_operation_ended_count() const {
    return batch_operation_ended_count_;
  }

  // Returns whether WebStateListDestroyed was invoked.
  bool web_state_list_destroyed() const {
    return web_state_list_destroyed_count_ != 0;
  }

  // Returns the number of times WebStateListDestroyed was invoked.
  int web_state_list_destroyed_count() const {
    return web_state_list_destroyed_count_;
  }

  // WebStateListObserver implementation.
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override {
    switch (change.type()) {
      case WebStateListChange::Type::kStatusOnly: {
        const WebStateListChangeStatusOnly& status_only_change =
            change.As<WebStateListChangeStatusOnly>();
        ++status_only_count_;
        if (status_only_change.pinned_state_changed()) {
          ++pinned_state_changed_count_;
        }
        status_only_web_state_ = status_only_change.web_state();
        status_only_old_group_ = status_only_change.old_group();
        status_only_new_group_ = status_only_change.new_group();
        // The activation is handled after this switch statement.
        break;
      }
      case WebStateListChange::Type::kDetach: {
        const auto& detach_change = change.As<WebStateListChangeDetach>();
        EXPECT_TRUE(web_state_list->IsMutating());
        web_state_detached_group_ = detach_change.group();
        ++web_state_detached_count_;
        break;
      }
      case WebStateListChange::Type::kMove: {
        EXPECT_TRUE(web_state_list->IsMutating());
        ++web_state_moved_count_;
        const auto& move_change = change.As<WebStateListChangeMove>();
        if (move_change.pinned_state_changed()) {
          ++pinned_state_changed_count_;
        }
        web_state_moved_old_group_ = move_change.old_group();
        web_state_moved_new_group_ = move_change.new_group();
        break;
      }
      case WebStateListChange::Type::kReplace:
        EXPECT_TRUE(web_state_list->IsMutating());
        ++web_state_replaced_count_;
        break;
      case WebStateListChange::Type::kInsert: {
        const auto& insert_change = change.As<WebStateListChangeInsert>();
        web_state_inserted_group_ = insert_change.group();
        EXPECT_TRUE(web_state_list->IsMutating());
        ++web_state_inserted_count_;
        break;
      }
      case WebStateListChange::Type::kGroupCreate: {
        const auto& group_create_change =
            change.As<WebStateListChangeGroupCreate>();
        group_created_group_ = group_create_change.created_group();
        ++group_created_count_;
        break;
      }
      case WebStateListChange::Type::kGroupVisualDataUpdate: {
        const auto& visual_data_update_change =
            change.As<WebStateListChangeGroupVisualDataUpdate>();
        EXPECT_TRUE(web_state_list->IsMutating());
        visual_data_updated_group_ = visual_data_update_change.updated_group();
        old_visual_data_ = visual_data_update_change.old_visual_data();
        ++visual_data_updated_count_;
        break;
      }
      case WebStateListChange::Type::kGroupMove: {
        const auto& group_move_change =
            change.As<WebStateListChangeGroupMove>();
        group_moved_group_ = group_move_change.moved_group();
        group_moved_from_range_ = group_move_change.moved_from_range();
        group_moved_to_range_ = group_move_change.moved_to_range();
        ++group_moved_count_;
        break;
      }
      case WebStateListChange::Type::kGroupDelete: {
        const auto& group_delete_change =
            change.As<WebStateListChangeGroupDelete>();
        group_deleted_group_ = group_delete_change.deleted_group();
        ++group_deleted_count_;
        break;
      }
    }

    if (status.active_web_state_change()) {
      ++web_state_activated_count_;
    }
  }

  void WillBeginBatchOperation(WebStateList* web_state_list) override {
    ++batch_operation_started_count_;
  }

  void BatchOperationEnded(WebStateList* web_state_list) override {
    ++batch_operation_ended_count_;
  }

  void WebStateListDestroyed(WebStateList* web_state_list) override {
    ++web_state_list_destroyed_count_;
    observation_.RemoveObservation(web_state_list);
  }

 private:
  int web_state_inserted_count_ = 0;
  raw_ptr<const TabGroup> web_state_inserted_group_ = nullptr;
  int web_state_moved_count_ = 0;
  raw_ptr<const TabGroup> web_state_moved_old_group_ = nullptr;
  raw_ptr<const TabGroup> web_state_moved_new_group_ = nullptr;
  int web_state_replaced_count_ = 0;
  int web_state_detached_count_ = 0;
  raw_ptr<const TabGroup> web_state_detached_group_ = nullptr;
  int web_state_activated_count_ = 0;
  int pinned_state_changed_count_ = 0;
  int status_only_count_ = 0;
  raw_ptr<web::WebState> status_only_web_state_ = nullptr;
  raw_ptr<const TabGroup> status_only_old_group_ = nullptr;
  raw_ptr<const TabGroup> status_only_new_group_ = nullptr;
  int group_created_count_ = 0;
  raw_ptr<const TabGroup> group_created_group_ = nullptr;
  int visual_data_updated_count_ = 0;
  raw_ptr<const TabGroup> visual_data_updated_group_ = nullptr;
  TabGroupVisualData old_visual_data_ = TabGroupVisualData();
  int group_moved_count_ = 0;
  raw_ptr<const TabGroup> group_moved_group_ = nullptr;
  TabGroupRange group_moved_from_range_ = TabGroupRange::InvalidRange();
  TabGroupRange group_moved_to_range_ = TabGroupRange::InvalidRange();
  int group_deleted_count_ = 0;
  raw_ptr<const TabGroup> group_deleted_group_ = nullptr;
  int batch_operation_started_count_ = 0;
  int batch_operation_ended_count_ = 0;
  int web_state_list_destroyed_count_ = 0;
  base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>
      observation_{this};
};

class MockWebStateObserver : public web::WebStateObserver {
 public:
  MockWebStateObserver() {}
  ~MockWebStateObserver() override {}

  MOCK_METHOD1(WebStateDestroyed, void(web::WebState*));
};

// A fake NavigationManager used to test opener-opened relationship in the
// WebStateList.
class FakeNavigationManager : public web::FakeNavigationManager {
 public:
  FakeNavigationManager() = default;

  FakeNavigationManager(const FakeNavigationManager&) = delete;
  FakeNavigationManager& operator=(const FakeNavigationManager&) = delete;

  // web::NavigationManager implementation.
  int GetLastCommittedItemIndex() const override {
    return last_committed_item_index;
  }

  bool CanGoBack() const override { return last_committed_item_index > 0; }

  bool CanGoForward() const override {
    return last_committed_item_index < INT_MAX;
  }

  void GoBack() override {
    DCHECK(CanGoBack());
    --last_committed_item_index;
  }

  void GoForward() override {
    DCHECK(CanGoForward());
    ++last_committed_item_index;
  }

  void GoToIndex(int index) override { last_committed_item_index = index; }

  int last_committed_item_index = 0;
};

// A WebStateListDelegate that records the last inserted/activated WebState.
class TestWebStateListDelegate final : public WebStateListDelegate {
 public:
  void ResetStatistics() {
    inserted_web_state_count_ = 0;
    activated_web_state_count_ = 0;

    last_inserted_web_state_ = nullptr;
    last_activated_web_state_ = nullptr;
  }

  int InsertedWebStateCount() const { return inserted_web_state_count_; }
  int ActivatedWebStateCount() const { return activated_web_state_count_; }

  web::WebState* LastInsertedWebState() { return last_inserted_web_state_; }
  web::WebState* LastActivatedWebState() { return last_activated_web_state_; }

  // WebStateListDelegate implementation.
  void WillAddWebState(web::WebState* web_state) final {
    ++inserted_web_state_count_;
    last_inserted_web_state_ = web_state;
  }
  void WillActivateWebState(web::WebState* web_state) final {
    ++activated_web_state_count_;
    last_activated_web_state_ = web_state;
  }

 private:
  int inserted_web_state_count_ = 0;
  int activated_web_state_count_ = 0;
  raw_ptr<web::WebState> last_inserted_web_state_;
  raw_ptr<web::WebState> last_activated_web_state_;
};

}  // namespace

class WebStateListTest : public PlatformTest {
 public:
  WebStateListTest() : web_state_list_(&delegate_) {
    observer_.Observe(&web_state_list_);
  }

  WebStateListTest(const WebStateListTest&) = delete;
  WebStateListTest& operator=(const WebStateListTest&) = delete;

 protected:
  TestWebStateListDelegate delegate_;
  WebStateList web_state_list_;
  WebStateListTestObserver observer_;

  std::unique_ptr<web::FakeWebState> CreateWebState(const char* url) {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    fake_web_state->SetCurrentURL(GURL(url));
    fake_web_state->SetNavigationManager(
        std::make_unique<FakeNavigationManager>());
    return fake_web_state;
  }

  void AppendNewWebState(const char* url) {
    AppendNewWebState(url, WebStateOpener());
  }

  void AppendNewWebState(const char* url, WebStateOpener opener) {
    web_state_list_.InsertWebState(
        CreateWebState(url),
        WebStateList::InsertionParams::Automatic().WithOpener(opener));
  }

  void AppendNewWebState(std::unique_ptr<web::FakeWebState> web_state) {
    web_state_list_.InsertWebState(std::move(web_state));
  }

  // Returns whether for each WebState in `web_state_list_` at an index `index`,
  // the values returned by `GetGroupOfWebStateAt(...)` for `index - 1`, `index`
  // and `index + 1` are consistent with the range for this WebState's group.
  bool RangesOfTabGroupsAreValid() const {
    for (int index = 0; index < web_state_list_.count(); ++index) {
      const TabGroup* current_group =
          web_state_list_.GetGroupOfWebStateAt(index);
      if (!current_group) {
        continue;
      }
      const TabGroupRange current_group_range = current_group->range();
      if (!current_group_range.contains(index)) {
        return false;
      }
      const TabGroup* prev_group =
          web_state_list_.ContainsIndex(index - 1)
              ? web_state_list_.GetGroupOfWebStateAt(index - 1)
              : nullptr;
      if (current_group != prev_group &&
          current_group_range.range_begin() != index) {
        // The current TabGroup differs from the previous but the start of the
        // range does not match the current index.
        return false;
      }
      const TabGroup* next_group =
          web_state_list_.ContainsIndex(index + 1)
              ? web_state_list_.GetGroupOfWebStateAt(index + 1)
              : nullptr;
      if (current_group != next_group &&
          current_group_range.range_end() != index + 1) {
        // The current TabGroup differs from the next but the end of the range
        // does not match the current index plus one.
        return false;
      }
    }
    return true;
  }
};

// Tests that empty() matches count() != 0.
TEST_F(WebStateListTest, IsEmpty) {
  EXPECT_EQ(0, web_state_list_.count());
  EXPECT_TRUE(web_state_list_.empty());

  AppendNewWebState(kURL0);

  ASSERT_GE(web_state_list_.count(), 1);
  EXPECT_EQ(delegate_.LastInsertedWebState(), web_state_list_.GetWebStateAt(0));
  EXPECT_EQ(delegate_.LastActivatedWebState(), nullptr);

  EXPECT_TRUE(observer_.web_state_inserted());
  ASSERT_EQ(1, web_state_list_.count());
  EXPECT_FALSE(web_state_list_.empty());
}

// Tests that inserting a single webstate works.
TEST_F(WebStateListTest, InsertUrlSingle) {
  AppendNewWebState(kURL0);

  ASSERT_GE(web_state_list_.count(), 1);
  EXPECT_EQ(delegate_.LastInsertedWebState(), web_state_list_.GetWebStateAt(0));
  EXPECT_EQ(delegate_.LastActivatedWebState(), nullptr);

  EXPECT_TRUE(observer_.web_state_inserted());
  ASSERT_EQ(1, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
}

// Tests that inserting multiple webstates puts them in the expected places.
TEST_F(WebStateListTest, InsertUrlMultiple) {
  web_state_list_.InsertWebState(CreateWebState(kURL0),
                                 WebStateList::InsertionParams::AtIndex(0));

  ASSERT_GE(web_state_list_.count(), 1);
  EXPECT_EQ(delegate_.LastInsertedWebState(), web_state_list_.GetWebStateAt(0));
  EXPECT_EQ(delegate_.LastActivatedWebState(), nullptr);

  web_state_list_.InsertWebState(CreateWebState(kURL1),
                                 WebStateList::InsertionParams::AtIndex(0));

  ASSERT_GE(web_state_list_.count(), 1);
  EXPECT_EQ(delegate_.LastInsertedWebState(), web_state_list_.GetWebStateAt(0));
  EXPECT_EQ(delegate_.LastActivatedWebState(), nullptr);

  web_state_list_.InsertWebState(CreateWebState(kURL2),
                                 WebStateList::InsertionParams::AtIndex(1));

  ASSERT_GE(web_state_list_.count(), 1);
  EXPECT_EQ(delegate_.LastInsertedWebState(), web_state_list_.GetWebStateAt(1));
  EXPECT_EQ(delegate_.LastActivatedWebState(), nullptr);

  EXPECT_TRUE(observer_.web_state_inserted());
  ASSERT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
}

// Tests webstate activation.
TEST_F(WebStateListTest, ActivateWebState) {
  AppendNewWebState(kURL0);
  EXPECT_EQ(nullptr, web_state_list_.GetActiveWebState());

  ASSERT_GE(web_state_list_.count(), 1);
  EXPECT_EQ(delegate_.LastInsertedWebState(), web_state_list_.GetWebStateAt(0));
  EXPECT_EQ(delegate_.LastActivatedWebState(), nullptr);

  web_state_list_.ActivateWebStateAt(0);

  ASSERT_GE(web_state_list_.count(), 1);
  EXPECT_EQ(delegate_.LastInsertedWebState(), web_state_list_.GetWebStateAt(0));
  EXPECT_EQ(delegate_.LastActivatedWebState(),
            web_state_list_.GetWebStateAt(0));

  EXPECT_TRUE(observer_.web_state_activated());
  ASSERT_EQ(1, web_state_list_.count());
  EXPECT_EQ(web_state_list_.GetWebStateAt(0),
            web_state_list_.GetActiveWebState());
}

// Tests activating a webstate as it is inserted.
TEST_F(WebStateListTest, InsertActivate) {
  web_state_list_.InsertWebState(
      CreateWebState(kURL0),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  ASSERT_GE(web_state_list_.count(), 1);
  EXPECT_EQ(delegate_.LastInsertedWebState(), web_state_list_.GetWebStateAt(0));
  EXPECT_EQ(delegate_.LastActivatedWebState(),
            web_state_list_.GetWebStateAt(0));

  EXPECT_TRUE(observer_.web_state_inserted());
  EXPECT_TRUE(observer_.web_state_activated());
  ASSERT_EQ(1, web_state_list_.count());
  EXPECT_EQ(web_state_list_.GetWebStateAt(0),
            web_state_list_.GetActiveWebState());
}

// Tests that activating a WebState sends the proper notification.
TEST_F(WebStateListTest, ActivateNotifies) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* b c"));

  observer_.ResetStatistics();
  web_state_list_.ActivateWebStateAt(1);

  EXPECT_EQ("| a b* c", builder.GetWebStateListDescription());
  EXPECT_EQ(1, observer_.status_only_count());
  EXPECT_EQ(1, observer_.web_state_activated_count());
  EXPECT_EQ(web_state_list_.GetWebStateAt(1),
            observer_.status_only_web_state());
  EXPECT_EQ(nullptr, observer_.status_only_old_group());
  EXPECT_EQ(nullptr, observer_.status_only_new_group());
}

// Tests finding a known webstate.
TEST_F(WebStateListTest, GetIndexOfWebState) {
  auto web_state_0 = CreateWebState(kURL0);
  web::WebState* target_web_state = web_state_0.get();
  auto other_web_state = CreateWebState(kURL1);

  // Target not yet in list.
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfWebState(target_web_state));

  AppendNewWebState(kURL2);
  AppendNewWebState(std::move(web_state_0));
  // Target in list at index 1.
  EXPECT_EQ(1, web_state_list_.GetIndexOfWebState(target_web_state));
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfWebState(other_web_state.get()));

  // Another webstate with the same URL as the target also in list.
  AppendNewWebState(kURL0);
  EXPECT_EQ(1, web_state_list_.GetIndexOfWebState(target_web_state));

  // Another webstate inserted before target; target now at index 2.
  web_state_list_.InsertWebState(CreateWebState(kURL3),
                                 WebStateList::InsertionParams::AtIndex(0));
  EXPECT_EQ(2, web_state_list_.GetIndexOfWebState(target_web_state));
}

// Tests finding a webstate by URL.
TEST_F(WebStateListTest, GetIndexOfWebStateWithURL) {
  // Empty list.
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfWebStateWithURL(GURL(kURL0)));

  // One webstate with a different URL in list.
  AppendNewWebState(kURL1);
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfWebStateWithURL(GURL(kURL0)));

  // Target URL at index 1.
  AppendNewWebState(kURL0);
  EXPECT_EQ(1, web_state_list_.GetIndexOfWebStateWithURL(GURL(kURL0)));

  // Another webstate with the target URL also at index 3.
  AppendNewWebState(kURL2);
  AppendNewWebState(kURL0);
  EXPECT_EQ(1, web_state_list_.GetIndexOfWebStateWithURL(GURL(kURL0)));
}

// Tests finding a non-active webstate by URL.
TEST_F(WebStateListTest, GetIndexOfInactiveWebStateWithURL) {
  // Empty list.
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfInactiveWebStateWithURL(GURL(kURL0)));

  // One webstate with a different URL in list.
  AppendNewWebState(kURL1);
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfInactiveWebStateWithURL(GURL(kURL0)));

  // Target URL at index 1.
  AppendNewWebState(kURL0);
  EXPECT_EQ(1, web_state_list_.GetIndexOfInactiveWebStateWithURL(GURL(kURL0)));

  // Activate webstate at index 1.
  web_state_list_.ActivateWebStateAt(1);
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfInactiveWebStateWithURL(GURL(kURL0)));
  // GetIndexOfWebStateWithURL still finds it.
  EXPECT_EQ(1, web_state_list_.GetIndexOfWebStateWithURL(GURL(kURL0)));

  // Another webstate with the target URL also at index 3.
  AppendNewWebState(kURL2);
  AppendNewWebState(kURL0);
  EXPECT_EQ(3, web_state_list_.GetIndexOfInactiveWebStateWithURL(GURL(kURL0)));
  EXPECT_EQ(1, web_state_list_.GetIndexOfWebStateWithURL(GURL(kURL0)));

  // Activate the webstate at index 2, so there the target URL is both before
  // and after the active webstate.
  web_state_list_.ActivateWebStateAt(2);
  EXPECT_EQ(1, web_state_list_.GetIndexOfInactiveWebStateWithURL(GURL(kURL0)));

  // Remove the webstate at index 1, so the only webstate with the target URL
  // is after the active webstate.
  web_state_list_.DetachWebStateAt(1);

  // Active webstate is now index 1, target URL is at index 2.
  EXPECT_EQ(2, web_state_list_.GetIndexOfInactiveWebStateWithURL(GURL(kURL0)));
}

// Tests that inserted webstates correctly inherit openers.
TEST_F(WebStateListTest, InsertInheritOpener) {
  AppendNewWebState(kURL0);
  web_state_list_.ActivateWebStateAt(0);
  EXPECT_TRUE(observer_.web_state_activated());
  ASSERT_EQ(1, web_state_list_.count());
  ASSERT_EQ(web_state_list_.GetWebStateAt(0),
            web_state_list_.GetActiveWebState());

  web_state_list_.InsertWebState(
      CreateWebState(kURL1),
      WebStateList::InsertionParams::Automatic().InheritOpener());

  ASSERT_EQ(2, web_state_list_.count());
  ASSERT_EQ(web_state_list_.GetActiveWebState(),
            web_state_list_.GetOpenerOfWebStateAt(1).opener);
}

// Tests moving webstates one place to the "right" (to a higher index).
TEST_F(WebStateListTest, MoveWebStateAtRightByOne) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Coherence check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(0, 1);

  EXPECT_TRUE(observer_.web_state_moved());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
}

// Tests moving webstates more than one place to the "right" (to a higher
// index).
TEST_F(WebStateListTest, MoveWebStateAtRightByMoreThanOne) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(0, 2);

  EXPECT_TRUE(observer_.web_state_moved());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
}

// Tests moving webstates one place to the "left" (to a lower index).
TEST_F(WebStateListTest, MoveWebStateAtLeftByOne) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(2, 1);

  EXPECT_TRUE(observer_.web_state_moved());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
}

// Tests moving webstates more than one place to the "left" (to a lower index).
TEST_F(WebStateListTest, MoveWebStateAtLeftByMoreThanOne) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(2, 0);

  EXPECT_TRUE(observer_.web_state_moved());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
}

// Tests "moving" webstates (calling MoveWebStateAt with the same source and
// destination indexes.
TEST_F(WebStateListTest, MoveWebStateAtSameIndex) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(2, 2);

  EXPECT_FALSE(observer_.web_state_moved());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
}

// Tests moving an active webstate.
TEST_F(WebStateListTest, MoveActiveWebState) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);
  web_state_list_.ActivateWebStateAt(1);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
  EXPECT_EQ(1, web_state_list_.active_index());

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(1, 2);

  EXPECT_TRUE(observer_.web_state_moved());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(2, web_state_list_.active_index());
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
}

// Tests replacing webstates.
TEST_F(WebStateListTest, ReplaceWebStateAt) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);

  // Sanity check before replacing WebState.
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());

  observer_.ResetStatistics();
  std::unique_ptr<web::WebState> old_web_state(
      web_state_list_.ReplaceWebStateAt(1, CreateWebState(kURL2)));

  EXPECT_EQ(delegate_.LastActivatedWebState(), nullptr);

  EXPECT_TRUE(observer_.web_state_replaced());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, old_web_state->GetVisibleURL().spec());
}

// Tests replacing an active webstate.
TEST_F(WebStateListTest, ReplaceActiveWebStateAt) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  web_state_list_.ActivateWebStateAt(1);

  // Sanity check before replacing WebState.
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(1, web_state_list_.active_index());

  observer_.ResetStatistics();
  std::unique_ptr<web::WebState> old_web_state(
      web_state_list_.ReplaceWebStateAt(1, CreateWebState(kURL2)));

  EXPECT_EQ(delegate_.LastActivatedWebState(),
            web_state_list_.GetWebStateAt(1));

  EXPECT_TRUE(observer_.web_state_replaced());
  EXPECT_TRUE(observer_.web_state_activated());
  EXPECT_EQ(1, web_state_list_.active_index());
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, old_web_state->GetVisibleURL().spec());
}

// Tests detaching webstates at index 0.
TEST_F(WebStateListTest, DetachWebStateAtIndexBeginning) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());

  observer_.ResetStatistics();
  web_state_list_.DetachWebStateAt(0);

  EXPECT_EQ(delegate_.LastActivatedWebState(), nullptr);

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
}

// Tests detaching webstates at an index that isn't 0 or the last index.
TEST_F(WebStateListTest, DetachWebStateAtIndexMiddle) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());

  observer_.ResetStatistics();
  web_state_list_.DetachWebStateAt(1);

  EXPECT_EQ(delegate_.LastActivatedWebState(), nullptr);

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
}

// Tests detaching webstates at the last index.
TEST_F(WebStateListTest, DetachWebStateAtIndexLast) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());

  observer_.ResetStatistics();
  web_state_list_.DetachWebStateAt(2);

  EXPECT_EQ(delegate_.LastActivatedWebState(), nullptr);

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
}

// Tests detaching an active webstate.
TEST_F(WebStateListTest, DetachActiveWebState) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);
  web_state_list_.ActivateWebStateAt(0);

  EXPECT_EQ(delegate_.LastActivatedWebState(),
            web_state_list_.GetActiveWebState());

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
  EXPECT_EQ(0, web_state_list_.active_index());

  observer_.ResetStatistics();
  web_state_list_.DetachWebStateAt(0);

  // Note: this is a different WebState.
  EXPECT_EQ(delegate_.LastActivatedWebState(),
            web_state_list_.GetActiveWebState());

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_TRUE(observer_.web_state_activated());
  EXPECT_EQ(0, web_state_list_.active_index());
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
}

// Tests closing all non-pinned webstates (pinned WebStates present).
TEST_F(WebStateListTest, CloseAllNonPinnedWebStates_PinnedWebStatesPresent) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  web_state_list_.SetWebStatePinnedAt(0, true);

  // Sanity checks before closing WebStates.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));
  EXPECT_TRUE(observer_.pinned_state_changed());

  observer_.ResetStatistics();
  CloseAllNonPinnedWebStates(web_state_list_, WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ(1, web_state_list_.count());
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all non-pinned webstates (non-pinned WebStates not present).
TEST_F(WebStateListTest,
       CloseAllNonPinnedWebStates_NonPinnedWebStatesNotPresent) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  web_state_list_.SetWebStatePinnedAt(0, true);
  web_state_list_.SetWebStatePinnedAt(1, true);
  web_state_list_.SetWebStatePinnedAt(2, true);

  // Sanity checks before closing WebStates.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(1));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(2));
  EXPECT_TRUE(observer_.pinned_state_changed());

  observer_.ResetStatistics();
  CloseAllNonPinnedWebStates(web_state_list_, WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(1));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(2));

  EXPECT_FALSE(observer_.web_state_detached());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all non-pinned webstates (pinned WebStates not present).
TEST_F(WebStateListTest, CloseAllNonPinnedWebStates_PinnedWebStatesNotPresent) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Sanity checks before closing WebStates.
  EXPECT_EQ(3, web_state_list_.count());

  observer_.ResetStatistics();
  CloseAllNonPinnedWebStates(web_state_list_, WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ(0, web_state_list_.count());

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all non-pinned webstates (pinned active WebState present).
TEST_F(WebStateListTest,
       CloseAllNonPinnedWebStates_PinnedActiveWebStatePresent) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  web_state_list_.SetWebStatePinnedAt(0, true);
  web_state_list_.ActivateWebStateAt(0);

  // Sanity checks before closing WebStates.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(0, web_state_list_.active_index());
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));
  EXPECT_TRUE(observer_.pinned_state_changed());

  observer_.ResetStatistics();
  CloseAllNonPinnedWebStates(web_state_list_, WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ(1, web_state_list_.count());
  EXPECT_EQ(0, web_state_list_.active_index());
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all non-pinned webstates (pinned WebState and active non-pinned
// WebState present independently).
TEST_F(WebStateListTest,
       CloseAllNonPinnedWebStates_PinnedWebStateAndActiveWebStatePresent) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  web_state_list_.SetWebStatePinnedAt(0, true);
  web_state_list_.ActivateWebStateAt(1);

  // Sanity checks before closing WebStates.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(1, web_state_list_.active_index());
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));
  EXPECT_TRUE(observer_.pinned_state_changed());

  observer_.ResetStatistics();
  CloseAllNonPinnedWebStates(web_state_list_, WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ(1, web_state_list_.count());
  EXPECT_EQ(0, web_state_list_.active_index());
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_TRUE(observer_.web_state_activated());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all grouped WebStates (non-grouped WebStates present).
TEST_F(WebStateListTest, CloseAllWebStatesInGroup_NonGroupedWebStatesPresent) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("a | b [ 0 c d ]"));
  const TabGroup* group = builder.GetTabGroupForIdentifier('0');

  observer_.ResetStatistics();
  CloseAllWebStatesInGroup(web_state_list_, group,
                           WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ("a | b", builder.GetWebStateListDescription());
  EXPECT_EQ(2, observer_.web_state_detached_count());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all grouped WebStates (non-grouped WebStates not present).
TEST_F(WebStateListTest,
       CloseAllWebStatesInGroup_NonGroupedWebStatesNotPresent) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a b c ]"));
  const TabGroup* group = builder.GetTabGroupForIdentifier('0');

  observer_.ResetStatistics();
  CloseAllWebStatesInGroup(web_state_list_, group,
                           WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ("|", builder.GetWebStateListDescription());
  EXPECT_EQ(3, observer_.web_state_detached_count());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all grouped WebStates (non-grouped active WebState present
// before the group).
TEST_F(WebStateListTest,
       CloseAllWebStatesInGroup_NonGroupedActiveWebStatePresentBefore) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* [ 0 b c ]"));
  const TabGroup* group = builder.GetTabGroupForIdentifier('0');

  observer_.ResetStatistics();
  CloseAllWebStatesInGroup(web_state_list_, group,
                           WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ("| a*", builder.GetWebStateListDescription());
  EXPECT_EQ(2, observer_.web_state_detached_count());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all grouped WebStates (non-grouped active WebState present
// after the group).
TEST_F(WebStateListTest,
       CloseAllWebStatesInGroup_NonGroupedActiveWebStatePresentAfter) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a b ] c*"));
  const TabGroup* group = builder.GetTabGroupForIdentifier('0');

  observer_.ResetStatistics();
  CloseAllWebStatesInGroup(web_state_list_, group,
                           WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ("| c*", builder.GetWebStateListDescription());
  EXPECT_EQ(2, observer_.web_state_detached_count());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all WebStates in group (non-grouped WebState and active grouped
// WebState present independently).
TEST_F(
    WebStateListTest,
    CloseAllWebStatesInGroup_NonGroupedWebStateAndActiveGroupedWebStatePresent) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a [ 0 b* c ] d"));
  const TabGroup* group = builder.GetTabGroupForIdentifier('0');

  observer_.ResetStatistics();
  CloseAllWebStatesInGroup(web_state_list_, group,
                           WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ("| a d*", builder.GetWebStateListDescription());
  EXPECT_EQ(2, observer_.web_state_detached_count());
  EXPECT_TRUE(observer_.web_state_activated());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all webstates (pinned and non-pinned).
TEST_F(WebStateListTest, CloseAllWebStates_PinnedNonPinned) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  web_state_list_.SetWebStatePinnedAt(0, true);
  web_state_list_.SetWebStatePinnedAt(1, true);

  // Sanity check before closing WebStates.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(1));
  EXPECT_TRUE(observer_.pinned_state_changed());

  observer_.ResetStatistics();
  CloseAllWebStates(web_state_list_, WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ(0, web_state_list_.count());

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all webstates (non-pinned).
TEST_F(WebStateListTest, CloseAllWebStates_NonPinned) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Sanity check before closing WebStates.
  EXPECT_EQ(3, web_state_list_.count());

  observer_.ResetStatistics();
  CloseAllWebStates(web_state_list_, WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ(0, web_state_list_.count());

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all webstates (pinned, non-pinned and active WebStates).
TEST_F(WebStateListTest, CloseAllWebStates_PinnedNonPinnedWithActiveWebState) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  web_state_list_.SetWebStatePinnedAt(0, true);
  web_state_list_.ActivateWebStateAt(1);

  // Sanity checks before closing WebStates.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(1, web_state_list_.active_index());
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));
  EXPECT_TRUE(observer_.pinned_state_changed());

  observer_.ResetStatistics();
  CloseAllWebStates(web_state_list_, WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ(0, web_state_list_.count());
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_.active_index());

  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_TRUE(observer_.web_state_activated());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all webstates (non-pinned) to verify WebStateObserver function
// invocation ordering (which can have performance implications).
TEST_F(WebStateListTest, CloseAllWebStates_ObserverNotificationOrder) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);

  ASSERT_EQ(2, web_state_list_.count());

  web::WebState* web_state1 = web_state_list_.GetWebStateAt(0);
  web::WebState* web_state2 = web_state_list_.GetWebStateAt(1);

  MockWebStateObserver observer1;
  MockWebStateObserver observer2;

  base::ScopedObservation<web::WebState, web::WebStateObserver> observation1(
      &observer1);
  base::ScopedObservation<web::WebState, web::WebStateObserver> observation2(
      &observer2);

  observation1.Observe(web_state1);
  observation2.Observe(web_state2);

  EXPECT_CALL(observer1, WebStateDestroyed(web_state1))
      .WillOnce([&](web::WebState*) {
        // All webstates should be detached before invoking WebStateDestroyed
        // for any of them.
        EXPECT_EQ(0, web_state_list_.count());
        EXPECT_TRUE(observer_.web_state_detached());
        EXPECT_TRUE(observer_.batch_operation_started());
        EXPECT_FALSE(observer_.batch_operation_ended());
        observation1.Reset();
      });

  EXPECT_CALL(observer2, WebStateDestroyed(web_state2))
      .WillOnce([&](web::WebState*) {
        // All webstates should be detached before invoking WebStateDestroyed
        // for any of them.
        EXPECT_EQ(0, web_state_list_.count());
        EXPECT_TRUE(observer_.web_state_detached());
        EXPECT_TRUE(observer_.batch_operation_started());
        EXPECT_FALSE(observer_.batch_operation_ended());
        observation2.Reset();
      });

  CloseAllWebStates(web_state_list_, WebStateList::CLOSE_USER_ACTION);

  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing a non-continuous range of WebStates.
TEST_F(WebStateListTest, CloseWebStatesAtIndices) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);
  AppendNewWebState(kURL3);
  AppendNewWebState(kURL4);
  AppendNewWebState(kURL5);
  AppendNewWebState(kURL6);

  web_state_list_.ActivateWebStateAt(3);

  // Sanity check before closing WebStates.
  EXPECT_EQ(7, web_state_list_.count());
  EXPECT_EQ(3, web_state_list_.active_index());

  delegate_.ResetStatistics();
  observer_.ResetStatistics();
  web_state_list_.CloseWebStatesAtIndices(WebStateList::CLOSE_USER_ACTION,
                                          RemovingIndexes{2, 3, 4, 6});

  // Check that the correct elements have been closed, and that the
  // active WebState is the expected one.
  ASSERT_EQ(3, web_state_list_.count());
  EXPECT_EQ(2, web_state_list_.active_index());
  EXPECT_EQ(web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec(), kURL0);
  EXPECT_EQ(web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec(), kURL1);
  EXPECT_EQ(web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec(), kURL5);

  // Check the delegate has only been called once, with the expected WebState
  // and that the observer has been called exactly once per removed WebState.
  EXPECT_EQ(delegate_.LastActivatedWebState(),
            web_state_list_.GetWebStateAt(2));
  EXPECT_EQ(1, delegate_.ActivatedWebStateCount());
  EXPECT_EQ(1, observer_.web_state_activated_count());
  EXPECT_EQ(4, observer_.web_state_detached_count());
}

// Tests closing one webstate.
TEST_F(WebStateListTest, CloseWebState) {
  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());

  observer_.ResetStatistics();
  web_state_list_.CloseWebStateAt(0, WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_TRUE(observer_.web_state_detached());
  EXPECT_FALSE(observer_.batch_operation_started());
  EXPECT_FALSE(observer_.batch_operation_ended());
}

// Tests that batch operation can do nothing.
TEST_F(WebStateListTest, StartBatchOperation_DoNothing) {
  observer_.ResetStatistics();

  {
    WebStateList::ScopedBatchOperation lock =
        web_state_list_.StartBatchOperation();
  }

  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests that IsBatchInProgress() returns the correct value.
TEST_F(WebStateListTest, StartBatchOperation_IsBatchInProgress) {
  EXPECT_FALSE(web_state_list_.IsBatchInProgress());

  {
    WebStateList::ScopedBatchOperation lock =
        web_state_list_.StartBatchOperation();
    EXPECT_TRUE(web_state_list_.IsBatchInProgress());
  }

  EXPECT_FALSE(web_state_list_.IsBatchInProgress());
}

// Tests WebStates are pinned correctly while their order in the WebStateList
// doesn't change.
TEST_F(WebStateListTest, SetWebStatePinned_KeepingExistingOrder) {
  EXPECT_TRUE(web_state_list_.empty());

  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);
  AppendNewWebState(kURL3);

  // Pin kURL0 WebState.
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(0, true), 0);
  // Pin kURL1 WebState.
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(1, true), 1);
  // Pin kURL2 WebState.
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(2, true), 2);

  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(1));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(2));
  EXPECT_FALSE(web_state_list_.IsWebStatePinnedAt(3));

  EXPECT_EQ(web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec(), kURL0);
  EXPECT_EQ(web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec(), kURL1);
  EXPECT_EQ(web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec(), kURL2);
  EXPECT_EQ(web_state_list_.GetWebStateAt(3)->GetVisibleURL().spec(), kURL3);
}

// Tests WebStates are pinned correctly while their order in the WebStateList
// change.
TEST_F(WebStateListTest, SetWebStatePinned_InRandomOrder) {
  EXPECT_TRUE(web_state_list_.empty());

  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);
  AppendNewWebState(kURL3);

  // Pin kURL2 WebState.
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(2, true), 0);
  // Pin kURL3 WebState.
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(3, true), 1);
  // Pin kURL0 WebState.
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(2, true), 2);
  // Unpin kURL3 WebState.
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(1, false), 3);

  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(1));
  EXPECT_FALSE(web_state_list_.IsWebStatePinnedAt(2));
  EXPECT_FALSE(web_state_list_.IsWebStatePinnedAt(3));

  EXPECT_EQ(web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec(), kURL2);
  EXPECT_EQ(web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec(), kURL0);
  EXPECT_EQ(web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec(), kURL1);
  EXPECT_EQ(web_state_list_.GetWebStateAt(3)->GetVisibleURL().spec(), kURL3);
}

// Tests pinned_tabs_count() and regular_tabs_count() return correct values.
TEST_F(WebStateListTest, PinnedAndRegularTabsCount) {
  EXPECT_TRUE(web_state_list_.empty());

  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);
  AppendNewWebState(kURL3);

  EXPECT_EQ(web_state_list_.pinned_tabs_count(), 0);
  EXPECT_EQ(web_state_list_.regular_tabs_count(), 4);

  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(0, true), 0);
  EXPECT_EQ(web_state_list_.pinned_tabs_count(), 1);
  EXPECT_EQ(web_state_list_.regular_tabs_count(), 3);

  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(3, true), 1);
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(3, true), 2);
  EXPECT_EQ(web_state_list_.pinned_tabs_count(), 3);
  EXPECT_EQ(web_state_list_.regular_tabs_count(), 1);

  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(3, true), 3);
  EXPECT_EQ(web_state_list_.pinned_tabs_count(), 4);
  EXPECT_EQ(web_state_list_.regular_tabs_count(), 0);

  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(0, false), 3);
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(0, false), 3);
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(0, false), 3);
  EXPECT_EQ(web_state_list_.pinned_tabs_count(), 1);
  EXPECT_EQ(web_state_list_.regular_tabs_count(), 3);

  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(0, false), 3);
  EXPECT_EQ(web_state_list_.pinned_tabs_count(), 0);
  EXPECT_EQ(web_state_list_.regular_tabs_count(), 4);
}

// Tests InsertWebState method correctly updates insertion index if it is in the
// pinned WebStates range.
TEST_F(WebStateListTest, InsertWebState_InsertionInPinnedRange) {
  const char testURL0[] = "https://chromium.org/test_0";
  const char testURL1[] = "https://chromium.org/test_1";
  const char testURL2[] = "https://chromium.org/test_2";

  EXPECT_TRUE(web_state_list_.empty());

  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);
  AppendNewWebState(kURL3);

  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(0, true), 0);
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(1, true), 1);
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(2, true), 2);

  // Insert a WebState into pinned WebStates range.
  web_state_list_.InsertWebState(CreateWebState(testURL0),
                                 WebStateList::InsertionParams::AtIndex(0));
  // Expect a WebState to be added at the end of the WebStateList.
  EXPECT_EQ(web_state_list_.GetWebStateAt(4)->GetVisibleURL().spec(), testURL0);

  // Insert a WebState into pinned WebStates range.
  web_state_list_.InsertWebState(CreateWebState(testURL1),
                                 WebStateList::InsertionParams::AtIndex(2));
  // Expect a WebState to be added at the end of the WebStateList.
  EXPECT_EQ(web_state_list_.GetWebStateAt(5)->GetVisibleURL().spec(), testURL1);

  // Insert a WebState into pinned WebStates range.
  web_state_list_.InsertWebState(CreateWebState(testURL2),
                                 WebStateList::InsertionParams::AtIndex(1));
  // Expect a WebState to be added at the end of the WebStateList.
  EXPECT_EQ(web_state_list_.GetWebStateAt(6)->GetVisibleURL().spec(), testURL2);
}

// Tests InsertWebState method correctly updates insertion index when the params
// specify it should be pinned.
TEST_F(WebStateListTest, InsertWebState_InsertWebStatePinned) {
  const char testURL0[] = "https://chromium.org/test_0";
  const char testURL1[] = "https://chromium.org/test_1";
  const char testURL2[] = "https://chromium.org/test_2";

  EXPECT_TRUE(web_state_list_.empty());

  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);
  AppendNewWebState(kURL3);

  // Insert a pinned WebState without specifying an index.
  web_state_list_.InsertWebState(
      CreateWebState(testURL0),
      WebStateList::InsertionParams::Automatic().Pinned());
  // Expect a WebState to be added into pinned WebStates range.
  EXPECT_EQ(web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec(), testURL0);
  // Expect a WebState to be pinned.
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));

  // Insert a pinned WebState to the non-pinned WebStates range.
  web_state_list_.InsertWebState(
      CreateWebState(testURL1),
      WebStateList::InsertionParams::AtIndex(2).Pinned());
  // Expect a WebState to be added at the end of the pinned WebStates range.
  EXPECT_EQ(web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec(), testURL1);
  // Expect a WebState to be pinned.
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(1));

  // Insert a pinned WebState to the pinned WebStates range.
  web_state_list_.InsertWebState(
      CreateWebState(testURL2),
      WebStateList::InsertionParams::AtIndex(0).Pinned());
  // Expect a WebState to be added at the end of the pinned WebStates range.
  EXPECT_EQ(web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec(), testURL2);
  // Expect a WebState to be pinned.
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));

  // Final check that only first three WebStates were pinned.
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(0));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(1));
  EXPECT_TRUE(web_state_list_.IsWebStatePinnedAt(2));
  EXPECT_FALSE(web_state_list_.IsWebStatePinnedAt(3));
}

// Tests MoveWebStateAt method moves the pinned WebStates within pinned
// WebStates range only.
TEST_F(WebStateListTest, MoveWebStateAt_KeepsPinnedWebStateWithinPinnedRange) {
  EXPECT_TRUE(web_state_list_.empty());

  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);
  AppendNewWebState(kURL3);

  // Pin first three WebStates.
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(0, true), 0);
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(1, true), 1);
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(2, true), 2);

  // Check the WebStates order.
  EXPECT_EQ(web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec(), kURL0);
  EXPECT_EQ(web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec(), kURL1);
  EXPECT_EQ(web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec(), kURL2);
  EXPECT_EQ(web_state_list_.GetWebStateAt(3)->GetVisibleURL().spec(), kURL3);

  // Try to move first pinned WebState contains of the pinned WebStates range.
  web_state_list_.MoveWebStateAt(0, 2);

  // Try to move first pinned WebState outside of the pinned WebStates range.
  web_state_list_.MoveWebStateAt(0, 3);

  // Expect the pinned WebStates to be moved within pinned WebStates range only.
  EXPECT_EQ(web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec(), kURL2);
  EXPECT_EQ(web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec(), kURL0);
  EXPECT_EQ(web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec(), kURL1);
  EXPECT_EQ(web_state_list_.GetWebStateAt(3)->GetVisibleURL().spec(), kURL3);
}

// Tests MoveWebStateAt method moves the non-pinned WebStates within non-pinned
// WebStates range only.
TEST_F(WebStateListTest,
       MoveWebStateAt_KeepsNonPinnedWebStatesWithinNonPinnedRange) {
  EXPECT_TRUE(web_state_list_.empty());

  AppendNewWebState(kURL0);
  AppendNewWebState(kURL1);
  AppendNewWebState(kURL2);
  AppendNewWebState(kURL3);

  // Pin first two WebStates.
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(0, true), 0);
  EXPECT_EQ(web_state_list_.SetWebStatePinnedAt(1, true), 1);

  // Check WebStates order.
  EXPECT_EQ(web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec(), kURL0);
  EXPECT_EQ(web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec(), kURL1);
  EXPECT_EQ(web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec(), kURL2);
  EXPECT_EQ(web_state_list_.GetWebStateAt(3)->GetVisibleURL().spec(), kURL3);

  // Try to move first non-pinned WebState inside of the non-pinned WebStates
  // range.
  web_state_list_.MoveWebStateAt(2, 3);

  // Try to move first non-pinned WebState to the pinned WebStates range.
  web_state_list_.MoveWebStateAt(2, 1);

  // Expect the non-pinned WebStates to be moved within non-pinned WebStates
  // range only.
  EXPECT_EQ(web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec(), kURL0);
  EXPECT_EQ(web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec(), kURL1);
  EXPECT_EQ(web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec(), kURL3);
  EXPECT_EQ(web_state_list_.GetWebStateAt(3)->GetVisibleURL().spec(), kURL2);
}

TEST_F(WebStateListTest, WebStateListDestroyed) {
  // Using a local WebStateList to observe its destruction.
  std::unique_ptr<WebStateList> web_state_list =
      std::make_unique<WebStateList>(&delegate_);
  observer_.Observe(web_state_list.get());
  EXPECT_FALSE(observer_.web_state_list_destroyed());
  web_state_list.reset();
  EXPECT_TRUE(observer_.web_state_list_destroyed());
}

TEST_F(WebStateListTest, WebStateListAsWeakPtr) {
  // Using a local WebStateList to observe its destruction.
  std::unique_ptr<WebStateList> web_state_list =
      std::make_unique<WebStateList>(&delegate_);
  base::WeakPtr<WebStateList> weak_web_state_list = web_state_list->AsWeakPtr();
  EXPECT_TRUE(weak_web_state_list);
  web_state_list.reset();
  EXPECT_FALSE(weak_web_state_list);
}

// Tests that GetGroupOfWebStateAt returns the correct group(s).
TEST_F(WebStateListTest, GetGroupOfWebStateAt) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("a b | [ 0 c d ] e"));

  EXPECT_EQ(nullptr, web_state_list_.GetGroupOfWebStateAt(0));
  EXPECT_EQ(nullptr, web_state_list_.GetGroupOfWebStateAt(1));
  const TabGroup* group = web_state_list_.GetGroupOfWebStateAt(2);
  EXPECT_NE(nullptr, group);
  EXPECT_EQ(group, web_state_list_.GetGroupOfWebStateAt(3));
  EXPECT_EQ(nullptr, web_state_list_.GetGroupOfWebStateAt(4));
}

// Tests that groups return the correct ranges.
TEST_F(WebStateListTest, GetGroupRanges) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "a b | c [ 0 d ] e [ 1 f g h ] [ 2 i ] j"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');
  const TabGroup* group_2 = builder.GetTabGroupForIdentifier('2');

  EXPECT_EQ(TabGroupRange(3, 1), group_0->range());
  EXPECT_EQ(TabGroupRange(5, 3), group_1->range());
  EXPECT_EQ(TabGroupRange(8, 1), group_2->range());
}

// Tests that inserting when there are no groups doesn't create any group.
TEST_F(WebStateListTest, InsertWebState_NoGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a*"));

  web_state_list_.InsertWebState(CreateWebState(kURL1));

  EXPECT_EQ("| a* _", builder.GetWebStateListDescription());
}

// Checks that using `InsertWebState()` on a WebStateList with groups yields the
// expected result when inserting at an automatically determined index.
TEST_F(WebStateListTest, InsertWebState_Groups_Automatic) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("a | b [ 0 c ]"));

  auto web_state_x = CreateWebState(kURL0);
  auto web_state_y = CreateWebState(kURL1);
  auto web_state_z = CreateWebState(kURL2);
  builder.SetWebStateIdentifier(web_state_x.get(), 'x');
  builder.SetWebStateIdentifier(web_state_y.get(), 'y');
  builder.SetWebStateIdentifier(web_state_z.get(), 'z');

  ASSERT_EQ("a | b [ 0 c ]", builder.GetWebStateListDescription());
  web_state_list_.InsertWebState(std::move(web_state_x),
                                 WebStateList::InsertionParams::Automatic());
  EXPECT_EQ("a | b [ 0 c ] x", builder.GetWebStateListDescription());
  web_state_list_.InsertWebState(std::move(web_state_y),
                                 WebStateList::InsertionParams::Automatic());
  EXPECT_EQ("a | b [ 0 c ] x y", builder.GetWebStateListDescription());
  web_state_list_.InsertWebState(std::move(web_state_z),
                                 WebStateList::InsertionParams::Automatic());
  EXPECT_EQ("a | b [ 0 c ] x y z", builder.GetWebStateListDescription());
}

// Checks that using `InsertWebState()` on a WebStateList with groups yields the
// expected result when inserting at different indices.
TEST_F(WebStateListTest, InsertWebState_Groups_AtIndex) {
  constexpr std::string_view web_state_list_description_before_insertion =
      "a | b [ 0 c d ] e f [ 1 g ]";
  constexpr std::string_view expected_description_for_insertion_index[]{
      "a | b [ 0 c d ] e f [ 1 g ] X",  // Insertion at 'a'.
      "a | X b [ 0 c d ] e f [ 1 g ]",  // Insertion at 'b'.
      "a | b X [ 0 c d ] e f [ 1 g ]",  // Insertion at 'c'.
      "a | b [ 0 c X d ] e f [ 1 g ]",  // Insertion at 'd',.
      "a | b [ 0 c d ] X e f [ 1 g ]",  // Insertion at 'e'.
      "a | b [ 0 c d ] e X f [ 1 g ]",  // Insertion at 'f'.
      "a | b [ 0 c d ] e f X [ 1 g ]",  // Insertion at 'g'.
      "a | b [ 0 c d ] e f [ 1 g ] X",  // Insertion after 'g'.
  };

  for (int insertion_index = 0;
       insertion_index < std::ssize(expected_description_for_insertion_index);
       ++insertion_index) {
    // Setting up WebStateList and WebState to insert.
    WebStateListBuilderFromDescription builder(&web_state_list_);
    ASSERT_TRUE(builder.BuildWebStateListFromDescription(
        web_state_list_description_before_insertion));
    observer_.ResetStatistics();
    std::unique_ptr<web::WebState> web_state_to_insert = CreateWebState(kURL0);
    web::WebState* web_state_to_insert_ptr = web_state_to_insert.get();
    builder.SetWebStateIdentifier(web_state_to_insert_ptr, 'X');
    ASSERT_TRUE(RangesOfTabGroupsAreValid());

    // Inserting the WebState at `insertion_index`.
    web_state_list_.InsertWebState(
        std::move(web_state_to_insert),
        WebStateList::InsertionParams::AtIndex(insertion_index));

    // Check everything is as expected after insertion.
    EXPECT_TRUE(RangesOfTabGroupsAreValid())
        << "\nContiguity of TabGroups broken in WebStateList after insertion."
        << "\nInsertion index: " << insertion_index
        << "\nDescription after insert: "
        << builder.GetWebStateListDescription();
    EXPECT_EQ(expected_description_for_insertion_index[insertion_index],
              builder.GetWebStateListDescription());
    int effective_insertion_index =
        web_state_list_.GetIndexOfWebState(web_state_to_insert_ptr);
    const TabGroup* insert_group =
        web_state_list_.GetGroupOfWebStateAt(effective_insertion_index);
    EXPECT_EQ(insert_group, observer_.web_state_inserted_group());
    EXPECT_EQ(1, observer_.web_state_inserted_count());

    // Resetting.
    CloseAllWebStates(web_state_list_, WebStateList::CLOSE_NO_FLAGS);
  }
}

// Checks that using `InsertWebState()` on a WebStateList with groups yields the
// expected result when inserting at an automatically determined index while
// setting the WebStateOpener manually.
TEST_F(WebStateListTest, InsertWebState_Groups_AutomaticWithOpener) {
  constexpr std::string_view web_state_list_description_before_insertion =
      "a b | c [ 0 d e ] f g [ 1 h ]";
  constexpr std::string_view expected_description_for_opener_index[]{
      "a b | c [ 0 d e ] f g [ 1 h ] X",  // Opener is 'a'.
      "a b | X c [ 0 d e ] f g [ 1 h ]",  // Opener is 'b'.
      "a b | c X [ 0 d e ] f g [ 1 h ]",  // Opener is 'c'.
      "a b | c [ 0 d X e ] f g [ 1 h ]",  // Opener is 'd'.
      "a b | c [ 0 d e X ] f g [ 1 h ]",  // Opener is 'e'.
      "a b | c [ 0 d e ] f X g [ 1 h ]",  // Opener is 'f'.
      "a b | c [ 0 d e ] f g X [ 1 h ]",  // Opener is 'g'.
      "a b | c [ 0 d e ] f g [ 1 h X ]",  // Opener is 'h'.
  };

  for (int opener_index = 0;
       opener_index < std::ssize(expected_description_for_opener_index);
       ++opener_index) {
    // Setting up WebStateList, opener and WebState to insert.
    WebStateListBuilderFromDescription builder(&web_state_list_);
    ASSERT_TRUE(builder.BuildWebStateListFromDescription(
        web_state_list_description_before_insertion));
    observer_.ResetStatistics();
    web::FakeWebState* opener_web_state = static_cast<web::FakeWebState*>(
        web_state_list_.GetWebStateAt(opener_index));
    opener_web_state->SetNavigationManager(
        std::make_unique<FakeNavigationManager>());
    std::unique_ptr<web::WebState> web_state_to_insert = CreateWebState(kURL0);
    web::WebState* web_state_to_insert_ptr = web_state_to_insert.get();
    builder.SetWebStateIdentifier(web_state_to_insert_ptr, 'X');
    ASSERT_TRUE(RangesOfTabGroupsAreValid());

    // Inserting the WebState with opener at `opener_index`.
    WebStateOpener opener(opener_web_state);
    web_state_list_.InsertWebState(
        std::move(web_state_to_insert),
        WebStateList::InsertionParams::Automatic().WithOpener(opener));

    // Check everything is as expected after insertion.
    EXPECT_TRUE(RangesOfTabGroupsAreValid())
        << "\nContiguity of TabGroups broken in WebStateList after insertion "
           "with opener."
        << "\nIndex of opener: " << opener_index
        << "\nDescription after insert: "
        << builder.GetWebStateListDescription();
    EXPECT_EQ(expected_description_for_opener_index[opener_index],
              builder.GetWebStateListDescription());
    int effective_insertion_index =
        web_state_list_.GetIndexOfWebState(web_state_to_insert_ptr);
    const TabGroup* insert_group =
        web_state_list_.GetGroupOfWebStateAt(effective_insertion_index);
    EXPECT_EQ(insert_group, observer_.web_state_inserted_group());
    EXPECT_EQ(1, observer_.web_state_inserted_count());

    // Resetting.
    CloseAllWebStates(web_state_list_, WebStateList::CLOSE_NO_FLAGS);
  }
}

// Checks that using `InsertWebState()` on a WebStateList with groups yields the
// expected result when inserting at an automatically determined index while
// inheriting the WebStateOpener i.e. using the currently active WebState as
// opener.
TEST_F(WebStateListTest, InsertWebState_Groups_AutomaticInheritOpener) {
  constexpr std::string_view web_state_list_description_before_insertion =
      "a b | c [ 0 d e ] f g [ 1 h ]";
  constexpr std::string_view expected_description_for_opener_index[]{
      "a* b | c [ 0 d e ] f g [ 1 h ] X",  // Opener is 'a'.
      "a b* | X c [ 0 d e ] f g [ 1 h ]",  // Opener is 'b'.
      "a b | c* X [ 0 d e ] f g [ 1 h ]",  // Opener is 'c'.
      "a b | c [ 0 d* X e ] f g [ 1 h ]",  // Opener is 'd'.
      "a b | c [ 0 d e* X ] f g [ 1 h ]",  // Opener is 'e'.
      "a b | c [ 0 d e ] f* X g [ 1 h ]",  // Opener is 'f'.
      "a b | c [ 0 d e ] f g* X [ 1 h ]",  // Opener is 'g'.
      "a b | c [ 0 d e ] f g [ 1 h* X ]",  // Opener is 'h'.
  };

  for (int opener_index = 0;
       opener_index < std::ssize(expected_description_for_opener_index);
       ++opener_index) {
    // Setting up WebStateList, opener and WebState to insert.
    WebStateListBuilderFromDescription builder(&web_state_list_);
    ASSERT_TRUE(builder.BuildWebStateListFromDescription(
        web_state_list_description_before_insertion));
    observer_.ResetStatistics();
    web::FakeWebState* opener_web_state = static_cast<web::FakeWebState*>(
        web_state_list_.GetWebStateAt(opener_index));
    opener_web_state->SetNavigationManager(
        std::make_unique<FakeNavigationManager>());
    web_state_list_.ActivateWebStateAt(opener_index);
    std::unique_ptr<web::WebState> web_state_to_insert = CreateWebState(kURL0);
    web::WebState* web_state_to_insert_ptr = web_state_to_insert.get();
    builder.SetWebStateIdentifier(web_state_to_insert_ptr, 'X');
    ASSERT_TRUE(RangesOfTabGroupsAreValid());

    // Inserting the WebState with opener at `opener_index`.
    WebStateOpener opener(opener_web_state);
    web_state_list_.InsertWebState(
        std::move(web_state_to_insert),
        WebStateList::InsertionParams::Automatic().InheritOpener());

    // Check everything is as expected after insertion.
    EXPECT_TRUE(RangesOfTabGroupsAreValid())
        << "\nContiguity of TabGroups broken in WebStateList after insertion "
           "with inherited opener."
        << "\nIndex of opener: " << opener_index
        << "\nDescription after insert: "
        << builder.GetWebStateListDescription();
    EXPECT_EQ(expected_description_for_opener_index[opener_index],
              builder.GetWebStateListDescription());
    int effective_insertion_index =
        web_state_list_.GetIndexOfWebState(web_state_to_insert_ptr);
    const TabGroup* insert_group =
        web_state_list_.GetGroupOfWebStateAt(effective_insertion_index);
    EXPECT_EQ(insert_group, observer_.web_state_inserted_group());
    EXPECT_EQ(1, observer_.web_state_inserted_count());

    // Resetting.
    CloseAllWebStates(web_state_list_, WebStateList::CLOSE_NO_FLAGS);
  }
}

// Checks that using `InsertWebState()` on a WebStateList with groups yields the
// expected result when inserting at an automatically determined index in a
// group.
TEST_F(WebStateListTest, InsertWebState_Groups_AutomaticInGroup) {
  constexpr std::string_view web_state_list_description_before_insertion =
      "a b | c [ 0 d e ] f g [ 1 h ] [ 2 i j k ]";
  const std::map<char, std::string_view>
      expected_description_for_group_identifier{
          {'0', "a b | c [ 0 d e X ] f g [ 1 h ] [ 2 i j k ]"},  // In group 0.
          {'1', "a b | c [ 0 d e ] f g [ 1 h X ] [ 2 i j k ]"},  // In group 1.
          {'2', "a b | c [ 0 d e ] f g [ 1 h ] [ 2 i j k X ]"},  // In group 2.
      };

  for (const auto& [group_identifier, expected_description] :
       expected_description_for_group_identifier) {
    // Setting up WebStateList, opener and WebState to insert.
    WebStateListBuilderFromDescription builder(&web_state_list_);
    ASSERT_TRUE(builder.BuildWebStateListFromDescription(
        web_state_list_description_before_insertion));
    observer_.ResetStatistics();
    std::unique_ptr<web::WebState> web_state_to_insert = CreateWebState(kURL0);
    web::WebState* web_state_to_insert_ptr = web_state_to_insert.get();
    builder.SetWebStateIdentifier(web_state_to_insert_ptr, 'X');
    ASSERT_TRUE(RangesOfTabGroupsAreValid());

    // Inserting the WebState with opener at `opener_index`.
    const TabGroup* insertion_group =
        builder.GetTabGroupForIdentifier(group_identifier);
    ASSERT_NE(nullptr, insertion_group);
    web_state_list_.InsertWebState(
        std::move(web_state_to_insert),
        WebStateList::InsertionParams::Automatic().InGroup(insertion_group));

    // Check everything is as expected after insertion.
    EXPECT_TRUE(RangesOfTabGroupsAreValid())
        << "\nContiguity of TabGroups broken in WebStateList after insertion "
           "in group."
        << "\nIdentifier of group: " << group_identifier
        << "\nDescription after insert: "
        << builder.GetWebStateListDescription();
    EXPECT_EQ(expected_description, builder.GetWebStateListDescription());
    int effective_insertion_index =
        web_state_list_.GetIndexOfWebState(web_state_to_insert_ptr);
    const TabGroup* effective_insertion_group =
        web_state_list_.GetGroupOfWebStateAt(effective_insertion_index);
    EXPECT_EQ(insertion_group, effective_insertion_group);
    EXPECT_EQ(insertion_group, observer_.web_state_inserted_group());
    EXPECT_EQ(1, observer_.web_state_inserted_count());

    // Resetting.
    CloseAllWebStates(web_state_list_, WebStateList::CLOSE_NO_FLAGS);
  }
}

// Checks that using `InsertWebState()` on a WebStateList with groups yields the
// expected result when inserting at different indices in a group with 1
// element.
TEST_F(WebStateListTest, InsertWebState_Groups_AtIndexInGroup1) {
  constexpr std::string_view web_state_list_description_before_insertion =
      "a b | c [ 1 d ] e f [ 2 g h ] [ 3 i j k ]";
  constexpr std::string_view expected_description_for_insertion_index[]{
      "a b | c [ 1 d X ] e f [ 2 g h ] [ 3 i j k ]",  // Insert at 'a'.
      "a b | c [ 1 d X ] e f [ 2 g h ] [ 3 i j k ]",  // Insert at 'b'.
      "a b | c [ 1 d X ] e f [ 2 g h ] [ 3 i j k ]",  // Insert at 'c'.
      "a b | c [ 1 X d ] e f [ 2 g h ] [ 3 i j k ]",  // Insert at 'd'.
      "a b | c [ 1 d X ] e f [ 2 g h ] [ 3 i j k ]",  // Insert at 'e'.
      "a b | c [ 1 d X ] e f [ 2 g h ] [ 3 i j k ]",  // Insert at 'f'.
      "a b | c [ 1 d X ] e f [ 2 g h ] [ 3 i j k ]",  // Insert at 'g'.
      "a b | c [ 1 d X ] e f [ 2 g h ] [ 3 i j k ]",  // Insert at 'h'.
      "a b | c [ 1 d X ] e f [ 2 g h ] [ 3 i j k ]",  // Insert at 'i'.
      "a b | c [ 1 d X ] e f [ 2 g h ] [ 3 i j k ]",  // Insert at 'j'.
      "a b | c [ 1 d X ] e f [ 2 g h ] [ 3 i j k ]",  // Insert at 'k'.
      "a b | c [ 1 d X ] e f [ 2 g h ] [ 3 i j k ]",  // Insert after 'k'.
  };

  for (int insertion_index = 0;
       insertion_index < std::ssize(expected_description_for_insertion_index);
       ++insertion_index) {
    // Setting up WebStateList, opener and WebState to insert.
    WebStateListBuilderFromDescription builder(&web_state_list_);
    ASSERT_TRUE(builder.BuildWebStateListFromDescription(
        web_state_list_description_before_insertion));
    observer_.ResetStatistics();
    std::unique_ptr<web::WebState> web_state_to_insert = CreateWebState(kURL0);
    web::WebState* web_state_to_insert_ptr = web_state_to_insert.get();
    builder.SetWebStateIdentifier(web_state_to_insert_ptr, 'X');
    ASSERT_TRUE(RangesOfTabGroupsAreValid());

    // Inserting the WebState at `insertion_index` in group 1.
    const TabGroup* insertion_group = builder.GetTabGroupForIdentifier('1');
    ASSERT_NE(nullptr, insertion_group);
    web_state_list_.InsertWebState(
        std::move(web_state_to_insert),
        WebStateList::InsertionParams::AtIndex(insertion_index)
            .InGroup(insertion_group));

    // Check everything is as expected after insertion.
    EXPECT_TRUE(RangesOfTabGroupsAreValid())
        << "\nContiguity of TabGroups broken in WebStateList after insertion "
           "in group 1."
        << "\nInsertion index: " << insertion_index
        << "\nDescription after insert: "
        << builder.GetWebStateListDescription();
    EXPECT_EQ(expected_description_for_insertion_index[insertion_index],
              builder.GetWebStateListDescription());
    int effective_insertion_index =
        web_state_list_.GetIndexOfWebState(web_state_to_insert_ptr);
    const TabGroup* effective_insertion_group =
        web_state_list_.GetGroupOfWebStateAt(effective_insertion_index);
    EXPECT_EQ(insertion_group, effective_insertion_group);
    EXPECT_EQ(insertion_group, observer_.web_state_inserted_group());
    EXPECT_EQ(1, observer_.web_state_inserted_count());

    // Resetting.
    CloseAllWebStates(web_state_list_, WebStateList::CLOSE_NO_FLAGS);
  }
}

// Checks that using `InsertWebState()` on a WebStateList with groups yields the
// expected result when inserting at different indices in a group with 2
// elements.
TEST_F(WebStateListTest, InsertWebState_Groups_AtIndexInGroup2) {
  constexpr std::string_view web_state_list_description_before_insertion =
      "a b | c [ 1 d ] e f [ 2 g h ] [ 3 i j k ]";
  constexpr std::string_view expected_description_for_insertion_index[]{
      "a b | c [ 1 d ] e f [ 2 g h X ] [ 3 i j k ]",  // Insert at 'a'.
      "a b | c [ 1 d ] e f [ 2 g h X ] [ 3 i j k ]",  // Insert at 'b'.
      "a b | c [ 1 d ] e f [ 2 g h X ] [ 3 i j k ]",  // Insert at 'c'.
      "a b | c [ 1 d ] e f [ 2 g h X ] [ 3 i j k ]",  // Insert at 'd'.
      "a b | c [ 1 d ] e f [ 2 g h X ] [ 3 i j k ]",  // Insert at 'e'.
      "a b | c [ 1 d ] e f [ 2 g h X ] [ 3 i j k ]",  // Insert at 'f'.
      "a b | c [ 1 d ] e f [ 2 X g h ] [ 3 i j k ]",  // Insert at 'g'.
      "a b | c [ 1 d ] e f [ 2 g X h ] [ 3 i j k ]",  // Insert at 'h'.
      "a b | c [ 1 d ] e f [ 2 g h X ] [ 3 i j k ]",  // Insert at 'i'.
      "a b | c [ 1 d ] e f [ 2 g h X ] [ 3 i j k ]",  // Insert at 'j'.
      "a b | c [ 1 d ] e f [ 2 g h X ] [ 3 i j k ]",  // Insert at 'k'.
      "a b | c [ 1 d ] e f [ 2 g h X ] [ 3 i j k ]",  // Insert after 'k'.
  };

  for (int insertion_index = 0;
       insertion_index < std::ssize(expected_description_for_insertion_index);
       ++insertion_index) {
    // Setting up WebStateList, opener and WebState to insert.
    WebStateListBuilderFromDescription builder(&web_state_list_);
    ASSERT_TRUE(builder.BuildWebStateListFromDescription(
        web_state_list_description_before_insertion));
    observer_.ResetStatistics();
    std::unique_ptr<web::WebState> web_state_to_insert = CreateWebState(kURL0);
    web::WebState* web_state_to_insert_ptr = web_state_to_insert.get();
    builder.SetWebStateIdentifier(web_state_to_insert_ptr, 'X');
    ASSERT_TRUE(RangesOfTabGroupsAreValid());

    // Inserting the WebState at `insertion_index` in group 1.
    const TabGroup* insertion_group = builder.GetTabGroupForIdentifier('2');
    ASSERT_NE(nullptr, insertion_group);
    web_state_list_.InsertWebState(
        std::move(web_state_to_insert),
        WebStateList::InsertionParams::AtIndex(insertion_index)
            .InGroup(insertion_group));

    // Check everything is as expected after insertion.
    EXPECT_TRUE(RangesOfTabGroupsAreValid())
        << "\nContiguity of TabGroups broken in WebStateList after insertion "
           "in group 2."
        << "\nInsertion index: " << insertion_index
        << "\nDescription after insert: "
        << builder.GetWebStateListDescription();
    EXPECT_EQ(expected_description_for_insertion_index[insertion_index],
              builder.GetWebStateListDescription());
    int effective_insertion_index =
        web_state_list_.GetIndexOfWebState(web_state_to_insert_ptr);
    const TabGroup* effective_insertion_group =
        web_state_list_.GetGroupOfWebStateAt(effective_insertion_index);
    EXPECT_EQ(insertion_group, effective_insertion_group);
    EXPECT_EQ(insertion_group, observer_.web_state_inserted_group());
    EXPECT_EQ(1, observer_.web_state_inserted_count());

    // Resetting.
    CloseAllWebStates(web_state_list_, WebStateList::CLOSE_NO_FLAGS);
  }
}

// Checks that using `InsertWebState()` on a WebStateList with groups yields the
// expected result when inserting at different indices in a group with 3
// elements.
TEST_F(WebStateListTest, InsertWebState_Groups_AtIndexInGroup3) {
  constexpr std::string_view web_state_list_description_before_insertion =
      "a b | c [ 1 d ] e f [ 2 g h ] [ 3 i j k ]";
  constexpr std::string_view expected_description_for_insertion_index[]{
      "a b | c [ 1 d ] e f [ 2 g h ] [ 3 i j k X ]",  // Insert at 'a'.
      "a b | c [ 1 d ] e f [ 2 g h ] [ 3 i j k X ]",  // Insert at 'b'.
      "a b | c [ 1 d ] e f [ 2 g h ] [ 3 i j k X ]",  // Insert at 'c'.
      "a b | c [ 1 d ] e f [ 2 g h ] [ 3 i j k X ]",  // Insert at 'd'.
      "a b | c [ 1 d ] e f [ 2 g h ] [ 3 i j k X ]",  // Insert at 'e'.
      "a b | c [ 1 d ] e f [ 2 g h ] [ 3 i j k X ]",  // Insert at 'f'.
      "a b | c [ 1 d ] e f [ 2 g h ] [ 3 i j k X ]",  // Insert at 'g'.
      "a b | c [ 1 d ] e f [ 2 g h ] [ 3 i j k X ]",  // Insert at 'h'.
      "a b | c [ 1 d ] e f [ 2 g h ] [ 3 X i j k ]",  // Insert at 'i'.
      "a b | c [ 1 d ] e f [ 2 g h ] [ 3 i X j k ]",  // Insert at 'j'.
      "a b | c [ 1 d ] e f [ 2 g h ] [ 3 i j X k ]",  // Insert at 'k'.
      "a b | c [ 1 d ] e f [ 2 g h ] [ 3 i j k X ]",  // Insert after 'k'.
  };

  for (int insertion_index = 0;
       insertion_index < std::ssize(expected_description_for_insertion_index);
       ++insertion_index) {
    // Setting up WebStateList, opener and WebState to insert.
    WebStateListBuilderFromDescription builder(&web_state_list_);
    ASSERT_TRUE(builder.BuildWebStateListFromDescription(
        web_state_list_description_before_insertion));
    observer_.ResetStatistics();
    std::unique_ptr<web::WebState> web_state_to_insert = CreateWebState(kURL0);
    web::WebState* web_state_to_insert_ptr = web_state_to_insert.get();
    builder.SetWebStateIdentifier(web_state_to_insert_ptr, 'X');
    ASSERT_TRUE(RangesOfTabGroupsAreValid());

    // Inserting the WebState at `insertion_index` in group 1.
    const TabGroup* insertion_group = builder.GetTabGroupForIdentifier('3');
    ASSERT_NE(nullptr, insertion_group);
    web_state_list_.InsertWebState(
        std::move(web_state_to_insert),
        WebStateList::InsertionParams::AtIndex(insertion_index)
            .InGroup(insertion_group));

    // Check everything is as expected after insertion.
    EXPECT_TRUE(RangesOfTabGroupsAreValid())
        << "\nContiguity of TabGroups broken in WebStateList after insertion "
           "in group 3."
        << "\nInsertion index: " << insertion_index
        << "\nDescription after insert: "
        << builder.GetWebStateListDescription();
    EXPECT_EQ(expected_description_for_insertion_index[insertion_index],
              builder.GetWebStateListDescription());
    int effective_insertion_index =
        web_state_list_.GetIndexOfWebState(web_state_to_insert_ptr);
    const TabGroup* effective_insertion_group =
        web_state_list_.GetGroupOfWebStateAt(effective_insertion_index);
    EXPECT_EQ(insertion_group, effective_insertion_group);
    EXPECT_EQ(insertion_group, observer_.web_state_inserted_group());
    EXPECT_EQ(1, observer_.web_state_inserted_count());

    // Resetting.
    CloseAllWebStates(web_state_list_, WebStateList::CLOSE_NO_FLAGS);
  }
}

TEST_F(WebStateListTest, InsertWebState_Grouped_Grouped) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a ]"));
  observer_.ResetStatistics();
  const TabGroup* group0 = builder.GetTabGroupForIdentifier('0');
  ASSERT_NE(nullptr, group0);

  observer_.ResetStatistics();
  std::unique_ptr<web::WebState> web_state_b = CreateWebState(kURL1);
  builder.SetWebStateIdentifier(web_state_b.get(), 'b');
  web_state_list_.InsertWebState(
      std::move(web_state_b),
      WebStateList::InsertionParams::Automatic().InGroup(group0));

  EXPECT_EQ(1, observer_.web_state_inserted());
  EXPECT_EQ(group0, observer_.web_state_inserted_group());
  EXPECT_EQ("| [ 0 a b ]", builder.GetWebStateListDescription());
}

// Checks that using `DetachWebStateAt()` on a WebStateList with groups yields
// the expected result when detaching at different indices.
TEST_F(WebStateListTest, DetachWebStateAt_Groups) {
  constexpr std::string_view web_state_list_description_before_detach =
      "a b | c [ 1 d ] e f [ 2 g h ] [ 3 i j k ]";
  constexpr std::string_view expected_description_for_detach_index[]{
      "b | c [ 1 d ] e f [ 2 g h ] [ 3 i j k ]",  // Detach 'a'.
      "a | c [ 1 d ] e f [ 2 g h ] [ 3 i j k ]",  // Detach 'b'.
      "a b | [ 1 d ] e f [ 2 g h ] [ 3 i j k ]",  // Detach 'c'.
      "a b | c e f [ 2 g h ] [ 3 i j k ]",        // Detach 'd'.
      "a b | c [ 1 d ] f [ 2 g h ] [ 3 i j k ]",  // Detach 'e'.
      "a b | c [ 1 d ] e [ 2 g h ] [ 3 i j k ]",  // Detach 'f'.
      "a b | c [ 1 d ] e f [ 2 h ] [ 3 i j k ]",  // Detach 'g'.
      "a b | c [ 1 d ] e f [ 2 g ] [ 3 i j k ]",  // Detach 'h'.
      "a b | c [ 1 d ] e f [ 2 g h ] [ 3 j k ]",  // Detach 'i'.
      "a b | c [ 1 d ] e f [ 2 g h ] [ 3 i k ]",  // Detach 'j'.
      "a b | c [ 1 d ] e f [ 2 g h ] [ 3 i j ]",  // Detach 'k'.
  };

  for (int detach_index = 0;
       detach_index < std::ssize(expected_description_for_detach_index);
       ++detach_index) {
    // Setting up WebStateList and WebState to insert.
    WebStateListBuilderFromDescription builder(&web_state_list_);
    ASSERT_TRUE(builder.BuildWebStateListFromDescription(
        web_state_list_description_before_detach));
    observer_.ResetStatistics();
    const TabGroup* group_before_detach =
        web_state_list_.GetGroupOfWebStateAt(detach_index);

    // Detach the WebState at `detach_index`.
    ASSERT_TRUE(RangesOfTabGroupsAreValid());
    web_state_list_.DetachWebStateAt(detach_index);
    EXPECT_TRUE(RangesOfTabGroupsAreValid())
        << "\nContiguity of TabGroups broken in WebStateList after detach."
        << "\nDetach index: " << detach_index << "\nDescription after detach: "
        << builder.GetWebStateListDescription();

    // Check everything is as expected after insertion.
    EXPECT_EQ(expected_description_for_detach_index[detach_index],
              builder.GetWebStateListDescription());
    EXPECT_EQ(1, observer_.web_state_detached_count());
    EXPECT_EQ(group_before_detach, observer_.web_state_detached_group());

    // Resetting.
    CloseAllWebStates(web_state_list_, WebStateList::CLOSE_NO_FLAGS);
  }
}

// Checks that detaching the last WebState of a group leads to the deletion of
// that group.
TEST_F(WebStateListTest, DetachWebStateAt_DeleteEmptyGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* [ 0 b ]"));
  const TabGroup* group = builder.GetTabGroupForIdentifier('0');

  observer_.ResetStatistics();
  web_state_list_.DetachWebStateAt(1);

  EXPECT_EQ("| a*", builder.GetWebStateListDescription());
  EXPECT_EQ(1, observer_.group_deleted_count());
  EXPECT_EQ(group, observer_.group_deleted_group());
}

// Checks that detaching a non-last WebState of a group doesn't lead to the
// deletion of that group.
TEST_F(WebStateListTest, DetachWebStateAt_DontDeleteNonEmptyGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* [ 0 b c ]"));

  observer_.ResetStatistics();
  web_state_list_.DetachWebStateAt(1);

  EXPECT_EQ("| a* [ 0 c ]", builder.GetWebStateListDescription());
  EXPECT_EQ(0, observer_.group_deleted_count());
}

// Tests that moving when there are no groups doesn't create any group.
TEST_F(WebStateListTest, MoveWebStateAt_NoGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a b* c d"));

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(1, 3);

  EXPECT_EQ("| a c d b*", builder.GetWebStateListDescription());
  EXPECT_EQ(1, observer_.web_state_moved_count());
  EXPECT_EQ(0, observer_.status_only_count());
  EXPECT_FALSE(observer_.pinned_state_changed());
  EXPECT_EQ(nullptr, observer_.web_state_moved_old_group());
  EXPECT_EQ(nullptr, observer_.web_state_moved_new_group());
}

// Tests that moving from a group to the same position keeps the group.
TEST_F(WebStateListTest, MoveWebStateAt_NoMove_Grouped) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a ]"));
  const TabGroup* group = builder.GetTabGroupForIdentifier('0');

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(0, 0);

  EXPECT_EQ("| [ 0 a ]", builder.GetWebStateListDescription());
  EXPECT_EQ(0, observer_.web_state_moved_count());
  EXPECT_EQ(0, observer_.status_only_count());
  EXPECT_EQ(TabGroupRange(0, 1), group->range());
}

// Tests that moving from a group to another position removes the group.
TEST_F(WebStateListTest, MoveWebStateAt_Move_GroupedToNoGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a ] b"));
  const TabGroup* group = builder.GetTabGroupForIdentifier('0');

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(0, 1);

  EXPECT_EQ("| b a", builder.GetWebStateListDescription());
  EXPECT_EQ(1, observer_.web_state_moved_count());
  EXPECT_EQ(0, observer_.status_only_count());
  EXPECT_EQ(group, observer_.web_state_moved_old_group());
  EXPECT_EQ(nullptr, observer_.web_state_moved_new_group());
}

// Tests that moving from a group to another position in the group keeps the
// group.
TEST_F(WebStateListTest, MoveWebStateAt_Move_GroupedToSameGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a b ]"));
  const TabGroup* group = builder.GetTabGroupForIdentifier('0');

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(0, 1);

  EXPECT_EQ("| [ 0 b a ]", builder.GetWebStateListDescription());
  EXPECT_EQ(1, observer_.web_state_moved_count());
  EXPECT_EQ(0, observer_.status_only_count());
  EXPECT_EQ(group, observer_.web_state_moved_old_group());
  EXPECT_EQ(group, observer_.web_state_moved_new_group());
  EXPECT_EQ(TabGroupRange(0, 2), group->range());
}

// Tests that moving from a group on the right to the middle of another group on
// the left moves the tab to that left group.
TEST_F(WebStateListTest, MoveWebStateAt_MoveLeft_GroupedToOtherGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(
      builder.BuildWebStateListFromDescription("| [ 0 a b ] [ 1 c d ]"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(2, 1);

  EXPECT_EQ("| [ 0 a c b ] [ 1 d ]", builder.GetWebStateListDescription());
  EXPECT_EQ(1, observer_.web_state_moved_count());
  EXPECT_EQ(0, observer_.status_only_count());
  EXPECT_EQ(group_1, observer_.web_state_moved_old_group());
  EXPECT_EQ(group_0, observer_.web_state_moved_new_group());
  EXPECT_EQ(TabGroupRange(0, 3), group_0->range());
  EXPECT_EQ(TabGroupRange(3, 1), group_1->range());
}

// Tests that moving from a group on the left to the middle of another group on
// the right moves the tab to that right group.
TEST_F(WebStateListTest, MoveWebStateAt_MoveRight_GroupedToOtherGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(
      builder.BuildWebStateListFromDescription("| [ 0 a b ] [ 1 c d ]"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(0, 2);

  EXPECT_EQ("| [ 0 b ] [ 1 c a d ]", builder.GetWebStateListDescription());
  EXPECT_EQ(1, observer_.web_state_moved_count());
  EXPECT_EQ(0, observer_.status_only_count());
  EXPECT_EQ(group_0, observer_.web_state_moved_old_group());
  EXPECT_EQ(group_1, observer_.web_state_moved_new_group());
  EXPECT_EQ(TabGroupRange(0, 1), group_0->range());
  EXPECT_EQ(TabGroupRange(1, 3), group_1->range());
}

// Tests moving a ungrouped tab to another ungrouped position on the left
// updates untouched groups in between.
TEST_F(WebStateListTest,
       MoveWebStateAt_MoveToLeft_Ungrouped_UpdatesGroupsInBetween) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(
      builder.BuildWebStateListFromDescription("| [ 0 a ] [ 1 b ] c [ 2 d ]"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');
  const TabGroup* group_2 = builder.GetTabGroupForIdentifier('2');

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(2, 1);

  EXPECT_EQ("| [ 0 a ] c [ 1 b ] [ 2 d ]",
            builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(0, 1), group_0->range());
  EXPECT_EQ(TabGroupRange(2, 1), group_1->range());
  EXPECT_EQ(TabGroupRange(3, 1), group_2->range());
  EXPECT_EQ(1, observer_.web_state_moved_count());
  EXPECT_EQ(0, observer_.status_only_count());
  EXPECT_EQ(nullptr, observer_.web_state_moved_old_group());
  EXPECT_EQ(nullptr, observer_.web_state_moved_new_group());
}

// Tests moving an ungrouped tab to another ungrouped position on the right
// updates untouched groups in between.
TEST_F(WebStateListTest,
       MoveWebStateAt_MoveToRight_Ungrouped_UpdatesGroupsInBetween) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(
      builder.BuildWebStateListFromDescription("| [ 0 a ] b [ 1 c ] [ 2 d ]"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');
  const TabGroup* group_2 = builder.GetTabGroupForIdentifier('2');

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(1, 2);

  EXPECT_EQ("| [ 0 a ] [ 1 c ] b [ 2 d ]",
            builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(0, 1), group_0->range());
  EXPECT_EQ(TabGroupRange(1, 1), group_1->range());
  EXPECT_EQ(TabGroupRange(3, 1), group_2->range());
  EXPECT_EQ(1, observer_.web_state_moved_count());
  EXPECT_EQ(0, observer_.status_only_count());
  EXPECT_EQ(nullptr, observer_.web_state_moved_old_group());
  EXPECT_EQ(nullptr, observer_.web_state_moved_new_group());
}

// Tests that replacing when there are no groups doesn't create any group.
TEST_F(WebStateListTest, ReplaceWebStateAt_NoGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a*"));
  auto replacement_web_state = CreateWebState(kURL1);

  web_state_list_.ReplaceWebStateAt(0, std::move(replacement_web_state));

  EXPECT_EQ("| a*", builder.GetWebStateListDescription());
}

// Tests that replacing when there is a group keeps the group.
TEST_F(WebStateListTest, ReplaceWebStateAt_Grouped) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a* ]"));
  auto replacement_web_state = CreateWebState(kURL1);

  web_state_list_.ReplaceWebStateAt(0, std::move(replacement_web_state));

  EXPECT_EQ("| [ 0 a* ]", builder.GetWebStateListDescription());
}

// Tests that activating a non-grouped WebState doesn't create any group.
TEST_F(WebStateListTest, ActivateWebStateAt_NoGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a"));

  observer_.ResetStatistics();
  web_state_list_.ActivateWebStateAt(0);

  EXPECT_EQ("| a*", builder.GetWebStateListDescription());
  EXPECT_EQ(1, observer_.status_only_count());
  EXPECT_EQ(1, observer_.web_state_activated_count());
  EXPECT_EQ(web_state_list_.GetWebStateAt(0),
            observer_.status_only_web_state());
  EXPECT_EQ(nullptr, observer_.status_only_old_group());
  EXPECT_EQ(nullptr, observer_.status_only_new_group());
}

// Tests that activating a grouped WebState keeps the group.
TEST_F(WebStateListTest, ActivateWebStateAt_Grouped) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a ]"));
  const TabGroup* group = builder.GetTabGroupForIdentifier('0');

  observer_.ResetStatistics();
  web_state_list_.ActivateWebStateAt(0);

  EXPECT_EQ("| [ 0 a* ]", builder.GetWebStateListDescription());
  EXPECT_EQ(1, observer_.web_state_activated_count());
  EXPECT_EQ(1, observer_.status_only_count());
  EXPECT_EQ(web_state_list_.GetWebStateAt(0),
            observer_.status_only_web_state());
  EXPECT_EQ(group, observer_.status_only_old_group());
  EXPECT_EQ(group, observer_.status_only_new_group());
}

// Tests that pinning a grouped tab updates removes the tab from the group.
TEST_F(WebStateListTest, SetWebStatePinnedAt_PinningUngroups) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a b ]"));
  const TabGroup* group = builder.GetTabGroupForIdentifier('0');

  observer_.ResetStatistics();
  web_state_list_.SetWebStatePinnedAt(0, true);

  EXPECT_EQ("a | [ 0 b ]", builder.GetWebStateListDescription());
  EXPECT_EQ(1, observer_.status_only_count());
  EXPECT_EQ(0, observer_.web_state_moved_count());
  EXPECT_TRUE(observer_.pinned_state_changed());
  EXPECT_EQ(group, observer_.status_only_old_group());
  EXPECT_EQ(nullptr, observer_.status_only_new_group());
  EXPECT_EQ(TabGroupRange(1, 1), group->range());
}

// Tests that unpinning a tab doesn't add it to a group.
TEST_F(WebStateListTest, SetWebStatePinnedAt_UnpinningDoesntGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("a | [ 0 b ]"));
  const TabGroup* group = builder.GetTabGroupForIdentifier('0');

  observer_.ResetStatistics();
  web_state_list_.SetWebStatePinnedAt(0, false);

  EXPECT_EQ("| [ 0 b ] a", builder.GetWebStateListDescription());
  EXPECT_EQ(0, observer_.status_only_count());
  EXPECT_EQ(1, observer_.web_state_moved_count());
  EXPECT_TRUE(observer_.pinned_state_changed());
  EXPECT_EQ(nullptr, observer_.web_state_moved_old_group());
  EXPECT_EQ(nullptr, observer_.web_state_moved_new_group());
  EXPECT_EQ(TabGroupRange(0, 1), group->range());
}

// Tests that getting groups returns the groups of the web state list.
TEST_F(WebStateListTest, GetGroups) {
  EXPECT_TRUE(web_state_list_.GetGroups().empty());

  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a b c ] d* e"));
  const TabGroup* group_0 = web_state_list_.GetGroupOfWebStateAt(0);
  EXPECT_EQ(1u, web_state_list_.GetGroups().size());
  EXPECT_EQ(group_0, *(web_state_list_.GetGroups().begin()));

  TabGroupVisualData visual_data =
      TabGroupVisualData(u"Group", tab_groups::TabGroupColorId::kPink);
  const TabGroup* group_1 =
      web_state_list_.CreateGroup({2}, visual_data, TabGroupId::GenerateNew());
  builder.SetTabGroupIdentifier(group_1, '1');

  EXPECT_EQ("| [ 0 a b ] [ 1 c ] d* e", builder.GetWebStateListDescription());
  EXPECT_EQ(2u, web_state_list_.GetGroups().size());
  EXPECT_TRUE(web_state_list_.GetGroups().contains(group_0));
  EXPECT_TRUE(web_state_list_.GetGroups().contains(group_1));

  web_state_list_.DetachWebStateAt(2);

  EXPECT_EQ("| [ 0 a b ] d* e", builder.GetWebStateListDescription());
  EXPECT_EQ(1u, web_state_list_.GetGroups().size());
  EXPECT_TRUE(web_state_list_.GetGroups().contains(group_0));
}

// Tests creating a group with one tab that doesn't move.
TEST_F(WebStateListTest, CreateGroup_OneTab_NotMoving) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a*"));
  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  TabGroupVisualData visual_data =
      TabGroupVisualData(u"Group", tab_groups::TabGroupColorId::kGrey);

  observer_.ResetStatistics();
  const TabGroup* group =
      web_state_list_.CreateGroup({0}, visual_data, tab_group_id);

  builder.SetTabGroupIdentifier(group, '0');
  EXPECT_EQ("| [ 0 a* ]", builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(0, 1), group->range());
  EXPECT_EQ(tab_group_id, group->tab_group_id());
  EXPECT_EQ(visual_data, group->visual_data());
  EXPECT_EQ(0, observer_.web_state_activated_count());
  EXPECT_EQ(0, observer_.web_state_moved_count());
  EXPECT_EQ(nullptr, observer_.status_only_old_group());
  EXPECT_EQ(group, observer_.status_only_new_group());
  EXPECT_EQ(1, observer_.group_created_count());
  EXPECT_EQ(group, observer_.group_created_group());
}

// Tests creating a group with one tab that moves.
TEST_F(WebStateListTest, CreateGroup_OneTab_Moving) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("a* b |"));
  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  TabGroupVisualData visual_data =
      TabGroupVisualData(u"Group", tab_groups::TabGroupColorId::kGrey);

  observer_.ResetStatistics();
  const TabGroup* group =
      web_state_list_.CreateGroup({0}, visual_data, tab_group_id);

  builder.SetTabGroupIdentifier(group, '0');
  EXPECT_EQ("b | [ 0 a* ]", builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(1, 1), group->range());
  EXPECT_EQ(tab_group_id, group->tab_group_id());
  EXPECT_EQ(visual_data, group->visual_data());
  EXPECT_EQ(0, observer_.web_state_activated_count());
  EXPECT_EQ(1, observer_.web_state_moved_count());
  EXPECT_EQ(nullptr, observer_.web_state_moved_old_group());
  EXPECT_EQ(group, observer_.web_state_moved_new_group());
  EXPECT_EQ(1, observer_.group_created_count());
  EXPECT_EQ(group, observer_.group_created_group());
}

// Tests creating a group with several tabs.
TEST_F(WebStateListTest, CreateGroup_SeveralTabs) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a b* c d e"));
  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  TabGroupVisualData visual_data =
      TabGroupVisualData(u"Group", tab_groups::TabGroupColorId::kGrey);

  observer_.ResetStatistics();
  const TabGroup* group =
      web_state_list_.CreateGroup({0, 2, 4}, visual_data, tab_group_id);

  builder.SetTabGroupIdentifier(group, '0');
  EXPECT_EQ("| [ 0 a c e ] b* d", builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(0, 3), group->range());
  EXPECT_EQ(tab_group_id, group->tab_group_id());
  EXPECT_EQ(visual_data, group->visual_data());
  EXPECT_EQ(0, observer_.web_state_activated_count());
  EXPECT_EQ(2, observer_.web_state_moved_count());
  EXPECT_EQ(nullptr, observer_.web_state_moved_old_group());
  EXPECT_EQ(group, observer_.web_state_moved_new_group());
  EXPECT_EQ(1, observer_.group_created_count());
  EXPECT_EQ(group, observer_.group_created_group());
}

// Tests creating a group with several tabs, some being pinned.
TEST_F(WebStateListTest, CreateGroup_SeveralTabs_SomePinned) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("a b* c | d e"));
  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  TabGroupVisualData visual_data =
      TabGroupVisualData(u"Group", tab_groups::TabGroupColorId::kGrey);

  observer_.ResetStatistics();
  const TabGroup* group =
      web_state_list_.CreateGroup({1, 3}, visual_data, tab_group_id);

  builder.SetTabGroupIdentifier(group, '0');
  EXPECT_EQ("a c | [ 0 b* d ] e", builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(2, 2), group->range());
  EXPECT_EQ(tab_group_id, group->tab_group_id());
  EXPECT_EQ(visual_data, group->visual_data());
  EXPECT_EQ(0, observer_.web_state_activated_count());
  EXPECT_EQ(1, observer_.web_state_moved_count());
  EXPECT_EQ(nullptr, observer_.web_state_moved_old_group());
  EXPECT_EQ(group, observer_.web_state_moved_new_group());
  EXPECT_EQ(1, observer_.group_created_count());
  EXPECT_EQ(group, observer_.group_created_group());
}

// Tests creating a group with several tabs, some being already grouped.
TEST_F(WebStateListTest, CreateGroup_SeveralTabs_SomeGrouped) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a b c ] d* e"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  TabGroupVisualData visual_data_1 =
      TabGroupVisualData(u"Group", tab_groups::TabGroupColorId::kBlue);

  observer_.ResetStatistics();
  const TabGroup* group_1 = web_state_list_.CreateGroup(
      {1, 3}, visual_data_1, TabGroupId::GenerateNew());

  builder.SetTabGroupIdentifier(group_1, '1');
  EXPECT_EQ("| [ 0 a c ] [ 1 b d* ] e", builder.GetWebStateListDescription());

  EXPECT_EQ(TabGroupRange(0, 2), group_0->range());
  EXPECT_EQ(TabGroupRange(2, 2), group_1->range());
  EXPECT_EQ(visual_data_1, group_1->visual_data());
  EXPECT_EQ(0, observer_.web_state_activated_count());
  EXPECT_EQ(1, observer_.web_state_moved_count());
  EXPECT_EQ(group_0, observer_.web_state_moved_old_group());
  EXPECT_EQ(group_1, observer_.web_state_moved_new_group());
  EXPECT_EQ(1, observer_.group_created_count());
  EXPECT_EQ(group_1, observer_.group_created_group());
}

TEST_F(WebStateListTest, CreateGroup_SeveralTabs_PinnedAndGrouped) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "a b c d e f | g h [ 0 i j k] l"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  TabGroupVisualData visual_data_1 =
      TabGroupVisualData(u"Group", tab_groups::TabGroupColorId::kBlue);

  observer_.ResetStatistics();
  const TabGroup* group_1 = web_state_list_.CreateGroup(
      {0, 1, 2, 3, 7, 8, 9}, visual_data_1, TabGroupId::GenerateNew());

  builder.SetTabGroupIdentifier(group_1, '1');
  EXPECT_EQ("e f | [ 1 a b c d h i j ] g [ 0 k ] l",
            builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(10, 1), group_0->range());
  EXPECT_EQ(TabGroupRange(2, 7), group_1->range());
  EXPECT_EQ(7, observer_.web_state_moved_count());
  EXPECT_EQ(1, observer_.group_created_count());
  EXPECT_EQ(group_1, observer_.group_created_group());
}

TEST_F(WebStateListTest, CreateGroup_SeveralTabs_GroupedLeftAndRight) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "| [ 0 a b c] d e f [ 1 g h i j ] k l"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');
  TabGroupVisualData visual_data_2 =
      TabGroupVisualData(u"Group", tab_groups::TabGroupColorId::kOrange);

  observer_.ResetStatistics();
  const TabGroup* group_2 = web_state_list_.CreateGroup(
      {1, 4, 7, 8}, visual_data_2, TabGroupId::GenerateNew());

  builder.SetTabGroupIdentifier(group_2, '2');
  EXPECT_EQ("| [ 0 a c ] [ 2 b e h i ] d f [ 1 g j ] k l",
            builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(0, 2), group_0->range());
  EXPECT_EQ(TabGroupRange(8, 2), group_1->range());
  EXPECT_EQ(TabGroupRange(2, 4), group_2->range());
  EXPECT_EQ(4, observer_.web_state_moved_count());
  EXPECT_EQ(1, observer_.group_created_count());
  EXPECT_EQ(group_2, observer_.group_created_group());
}

TEST_F(WebStateListTest, UpdateGroupVisualData_Changed) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a b ]"));
  const TabGroup* group = builder.GetTabGroupForIdentifier('0');
  const TabGroupVisualData original_visual_data = group->visual_data();
  EXPECT_EQ(original_visual_data, group->visual_data());
  const TabGroupVisualData new_visual_data =
      TabGroupVisualData(u"Group", tab_groups::TabGroupColorId::kOrange);
  EXPECT_NE(new_visual_data, group->visual_data());

  observer_.ResetStatistics();
  web_state_list_.UpdateGroupVisualData(group, new_visual_data);

  EXPECT_EQ("| [ 0 a b ]", builder.GetWebStateListDescription());
  EXPECT_EQ(new_visual_data, group->visual_data());
  EXPECT_EQ(1, observer_.visual_data_updated_count());
  EXPECT_EQ(group, observer_.visual_data_updated_group());
  EXPECT_EQ(original_visual_data, observer_.old_visual_data());
}

TEST_F(WebStateListTest, UpdateGroupVisualData_NoChange) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a b ]"));
  const TabGroup* group = builder.GetTabGroupForIdentifier('0');
  const TabGroupVisualData original_visual_data = group->visual_data();

  observer_.ResetStatistics();
  web_state_list_.UpdateGroupVisualData(group, original_visual_data);

  EXPECT_EQ("| [ 0 a b ]", builder.GetWebStateListDescription());
  EXPECT_EQ(original_visual_data, group->visual_data());
  EXPECT_EQ(0, observer_.visual_data_updated_count());
}

// Tests moving with same index but adding to the group on the left.
TEST_F(WebStateListTest, MoveToGroup_NoMove_GoToLeftGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a ] b "));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');

  observer_.ResetStatistics();
  web_state_list_.MoveToGroup({1}, group_0);

  EXPECT_EQ("| [ 0 a b ]", builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(0, 2), group_0->range());
  EXPECT_EQ(0, observer_.web_state_moved_count());
  EXPECT_EQ(1, observer_.status_only_count());
  EXPECT_EQ(nullptr, observer_.status_only_old_group());
  EXPECT_EQ(group_0, observer_.status_only_new_group());
}

// Tests keeping the same index but moving from own group to the group on the
// left (old group having no remaining tab in it).
TEST_F(WebStateListTest, MoveToGroup_NoMove_GoToLeftGroup_OldGroupEmpty) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a ] [ 1 b ]"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');

  observer_.ResetStatistics();
  web_state_list_.MoveToGroup({1}, group_0);

  EXPECT_EQ("| [ 0 a b ]", builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(0, 2), group_0->range());
  EXPECT_EQ(0, observer_.web_state_moved_count());
  EXPECT_EQ(1, observer_.status_only_count());
  EXPECT_EQ(group_1, observer_.status_only_old_group());
  EXPECT_EQ(group_0, observer_.status_only_new_group());
  EXPECT_EQ(1, observer_.group_deleted_count());
  EXPECT_EQ(group_1, observer_.group_deleted_group());
}

// Tests keeping the same index but moving from own group to the group on the
// left (old group still having remaining tab in it).
TEST_F(WebStateListTest, MoveToGroup_NoMove_GoToLeftGroup_OldGroupNonEmpty) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(
      builder.BuildWebStateListFromDescription("| [ 0 a b ] [ 1 c d ]"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');

  observer_.ResetStatistics();
  web_state_list_.MoveToGroup({2}, group_0);

  EXPECT_EQ("| [ 0 a b c ] [ 1 d ]", builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(0, 3), group_0->range());
  EXPECT_EQ(TabGroupRange(3, 1), group_1->range());
  EXPECT_EQ(0, observer_.web_state_moved_count());
  EXPECT_EQ(1, observer_.status_only_count());
  EXPECT_EQ(group_1, observer_.status_only_old_group());
  EXPECT_EQ(group_0, observer_.status_only_new_group());
}

// Tests moving a pinned tab to a group with a change of index.
TEST_F(WebStateListTest, MoveToGroup_Move_PinnedToGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("a | [ 0 b ]"));
  const TabGroup* group = builder.GetTabGroupForIdentifier('0');

  observer_.ResetStatistics();
  web_state_list_.MoveToGroup({0}, group);

  EXPECT_EQ("| [ 0 b a ]", builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(0, 2), group->range());
  EXPECT_EQ(1, observer_.web_state_moved_count());
  EXPECT_EQ(0, observer_.status_only_count());
  EXPECT_EQ(1, observer_.pinned_state_changed());
  EXPECT_EQ(nullptr, observer_.web_state_moved_old_group());
  EXPECT_EQ(group, observer_.web_state_moved_new_group());
}

// Tests moving an ungrouped tab to a group on its left.
TEST_F(WebStateListTest, MoveToGroup_MoveToLeft_NoGroupToGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a ] b c"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');

  observer_.ResetStatistics();
  web_state_list_.MoveToGroup({2}, group_0);

  EXPECT_EQ("| [ 0 a c ] b", builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(0, 2), group_0->range());
  EXPECT_EQ(1, observer_.web_state_moved_count());
  EXPECT_EQ(0, observer_.status_only_count());
  EXPECT_EQ(nullptr, observer_.web_state_moved_old_group());
  EXPECT_EQ(group_0, observer_.web_state_moved_new_group());
}

// Tests moving an ungrouped tab to a group on its right.
TEST_F(WebStateListTest, MoveToGroup_MoveToRight_NoGroupToGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a [ 0 b ]"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');

  observer_.ResetStatistics();
  web_state_list_.MoveToGroup({0}, group_0);

  EXPECT_EQ("| [ 0 b a ]", builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(0, 2), group_0->range());
  EXPECT_EQ(1, observer_.web_state_moved_count());
  EXPECT_EQ(0, observer_.status_only_count());
  EXPECT_EQ(nullptr, observer_.web_state_moved_old_group());
  EXPECT_EQ(group_0, observer_.web_state_moved_new_group());
}

// Tests moving a grouped tab to a group on its left (old group having no
// remaining tab in it).
TEST_F(WebStateListTest, MoveToGroup_MoveToLeft_GroupToGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a ] b [ 1 c ]"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');

  observer_.ResetStatistics();
  web_state_list_.MoveToGroup({2}, group_0);

  EXPECT_EQ("| [ 0 a c ] b", builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(0, 2), group_0->range());
  EXPECT_EQ(1, observer_.web_state_moved_count());
  EXPECT_EQ(0, observer_.status_only_count());
  EXPECT_EQ(group_1, observer_.web_state_moved_old_group());
  EXPECT_EQ(group_0, observer_.web_state_moved_new_group());
  EXPECT_EQ(1, observer_.group_deleted_count());
  EXPECT_EQ(group_1, observer_.group_deleted_group());
}

// Tests moving a grouped tab to a group on its right (old group having no
// remaining tab in it).
TEST_F(WebStateListTest, MoveToGroup_MoveToRight_GroupToGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a ] [ 1 b ]"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');

  observer_.ResetStatistics();
  web_state_list_.MoveToGroup({0}, group_1);

  EXPECT_EQ("| [ 1 b a ]", builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(0, 2), group_1->range());
  EXPECT_EQ(1, observer_.web_state_moved_count());
  EXPECT_EQ(0, observer_.status_only_count());
  EXPECT_EQ(group_0, observer_.web_state_moved_old_group());
  EXPECT_EQ(group_1, observer_.web_state_moved_new_group());
  EXPECT_EQ(1, observer_.group_deleted_count());
  EXPECT_EQ(group_0, observer_.group_deleted_group());
}

// Tests moving a grouped tab to a group on its left (old group still having
// remaining tabs in it).
TEST_F(WebStateListTest, MoveToGroup_MoveToLeft_GroupToGroup_NoEmptyGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a ] [ 1 b c ]"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');

  observer_.ResetStatistics();
  web_state_list_.MoveToGroup({2}, group_0);

  EXPECT_EQ("| [ 0 a c ] [ 1 b ]", builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(0, 2), group_0->range());
  EXPECT_EQ(TabGroupRange(2, 1), group_1->range());
  EXPECT_EQ(1, observer_.web_state_moved_count());
  EXPECT_EQ(0, observer_.status_only_count());
  EXPECT_EQ(group_1, observer_.web_state_moved_old_group());
  EXPECT_EQ(group_0, observer_.web_state_moved_new_group());
}

// Tests moving a grouped tab to a group on its right (old group still having
// remaining tabs in it).
TEST_F(WebStateListTest, MoveToGroup_MoveToRight_GroupToGroup_NoEmptyGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a b ] [ 1 c ]"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');

  observer_.ResetStatistics();
  web_state_list_.MoveToGroup({0}, group_1);

  EXPECT_EQ("| [ 0 b ] [ 1 c a ]", builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(0, 1), group_0->range());
  EXPECT_EQ(TabGroupRange(1, 2), group_1->range());
  EXPECT_EQ(1, observer_.web_state_moved_count());
  EXPECT_EQ(0, observer_.status_only_count());
  EXPECT_EQ(group_0, observer_.web_state_moved_old_group());
  EXPECT_EQ(group_1, observer_.web_state_moved_new_group());
}

// Tests removing ungrouped tabs from groups is a no-op.
TEST_F(WebStateListTest, RemoveFromGroups_NoGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a b c"));

  observer_.ResetStatistics();
  web_state_list_.RemoveFromGroups({0, 1, 2});

  EXPECT_EQ("| a b c", builder.GetWebStateListDescription());
  EXPECT_EQ(0, observer_.web_state_moved_count());
  EXPECT_EQ(0, observer_.status_only_count());
}

// Tests removing some tabs from a group ungroups them and moves them after the
// group (here they stay in place because they are already at the end). The
// group they were in still has tabs.
TEST_F(WebStateListTest, RemoveFromGroups_SomeFromSameGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(
      builder.BuildWebStateListFromDescription("| [0 a b c] [ 1 d e ]"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');

  observer_.ResetStatistics();
  web_state_list_.RemoveFromGroups({1, 2});

  EXPECT_EQ("| [ 0 a ] b c [ 1 d e ]", builder.GetWebStateListDescription());
  EXPECT_EQ(0, observer_.web_state_moved_count());
  EXPECT_EQ(2, observer_.status_only_count());
  EXPECT_EQ(group_0, observer_.status_only_old_group());
  EXPECT_EQ(nullptr, observer_.status_only_new_group());
  EXPECT_EQ(TabGroupRange(0, 1), group_0->range());
  EXPECT_EQ(TabGroupRange(3, 2), group_1->range());
}

// Tests removing all tabs from a group ungroups them and keeps them in place.
// The group they were in has no longer any tab.
TEST_F(WebStateListTest, RemoveFromGroups_AllFromSameGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(
      builder.BuildWebStateListFromDescription("| [0 a b c] [ 1 d e ]"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');

  observer_.ResetStatistics();
  web_state_list_.RemoveFromGroups({0, 1, 2});

  EXPECT_EQ("| a b c [ 1 d e ]", builder.GetWebStateListDescription());
  EXPECT_EQ(0, observer_.web_state_moved_count());
  EXPECT_EQ(3, observer_.status_only_count());
  EXPECT_EQ(group_0, observer_.status_only_old_group());
  EXPECT_EQ(nullptr, observer_.status_only_new_group());
  EXPECT_EQ(1, observer_.group_deleted_count());
  EXPECT_EQ(group_0, observer_.group_deleted_group());
  EXPECT_EQ(TabGroupRange(3, 2), group_1->range());
}

// Tests removing some tabs from different group ungroups them and moves them
// after their groups. The groups they were in still have tabs.
TEST_F(WebStateListTest, RemoveFromGroups_SomeFromDifferentGroupsWithMoves) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(
      builder.BuildWebStateListFromDescription("| [0 a b c] [ 1 d e ]"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');

  observer_.ResetStatistics();
  web_state_list_.RemoveFromGroups({1, 3});

  EXPECT_EQ("| [ 0 a c ] b [ 1 e ] d", builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(0, 2), group_0->range());
  EXPECT_EQ(TabGroupRange(3, 1), group_1->range());
  EXPECT_EQ(2, observer_.web_state_moved_count());
  EXPECT_EQ(0, observer_.status_only_count());
  // observer_.web_state_moved_old_group() is not sufficient to check the groups
  // communicated to observers because it only records the last move event, so
  // don't check here.
}

// Tests removing a pinned tab from its group is a no-op because a pinned tab
// has no group.
TEST_F(WebStateListTest, RemoveFromGroups_DoesntUnpin) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("a | [0 b]"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');

  observer_.ResetStatistics();
  web_state_list_.RemoveFromGroups({0, 1});

  EXPECT_EQ("a | b", builder.GetWebStateListDescription());
  EXPECT_EQ(0, observer_.web_state_moved_count());
  EXPECT_EQ(1, observer_.status_only_count());
  EXPECT_FALSE(observer_.pinned_state_changed());
  EXPECT_EQ(group_0, observer_.status_only_old_group());
  EXPECT_EQ(nullptr, observer_.status_only_new_group());
  EXPECT_EQ(1, observer_.group_deleted_count());
  EXPECT_EQ(group_0, observer_.group_deleted_group());
}

// Tests removing the active tab from its group keeps it active.
TEST_F(WebStateListTest, RemoveFromGroups_KeepsActive) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [0 a* b]"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');

  observer_.ResetStatistics();
  web_state_list_.RemoveFromGroups({0});

  EXPECT_EQ("| [ 0 b ] a*", builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(0, 1), group_0->range());
  EXPECT_EQ(1, observer_.web_state_moved_count());
  EXPECT_EQ(0, observer_.status_only_count());
  EXPECT_FALSE(observer_.pinned_state_changed());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(group_0, observer_.web_state_moved_old_group());
  EXPECT_EQ(nullptr, observer_.web_state_moved_new_group());
}

// Tests that moving a group to the same position is a no-op.
TEST_F(WebStateListTest, MoveGroup_NoMove_SamePosition) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(
      builder.BuildWebStateListFromDescription("a | [ 0 b* c ] [ 1 d e ]"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');

  observer_.ResetStatistics();
  web_state_list_.MoveGroup(group_0, 1);

  EXPECT_EQ("a | [ 0 b* c ] [ 1 d e ]", builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(1, 2), group_0->range());
  EXPECT_EQ(TabGroupRange(3, 2), group_1->range());
  EXPECT_EQ(0, observer_.web_state_moved_count());
  EXPECT_EQ(0, observer_.status_only_count());
  EXPECT_EQ(0, observer_.group_moved_count());
}

// Tests that moving a group to a position in itself is a no-op.
TEST_F(WebStateListTest, MoveGroup_NoMove_SameGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(
      builder.BuildWebStateListFromDescription("a | [ 0 b* c ] [ 1 d e ]"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');

  observer_.ResetStatistics();
  web_state_list_.MoveGroup(group_0, 2);

  EXPECT_EQ("a | [ 0 b* c ] [ 1 d e ]", builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(1, 2), group_0->range());
  EXPECT_EQ(TabGroupRange(3, 2), group_1->range());
  EXPECT_EQ(0, observer_.web_state_moved_count());
  EXPECT_EQ(0, observer_.status_only_count());
  EXPECT_EQ(0, observer_.group_moved_count());
}

// Tests MoveGroup on a WebStateList when moving a group at different indices.
// The active web state moves, as it's in the moving group.
TEST_F(WebStateListTest, MoveGroup_MovingActiveWebState) {
  // Group 2 is moving.
  constexpr std::string_view web_state_list_description_before_move =
      "a b | c [ 1 d e ] f g [ 2 h* i ] j [ 3 k l m ]";
  // Map of whether a GroupMove notification is sent, and the expected
  // description after the move.
  const std::vector<std::string_view> expected_description_for_move_index{
      "a b | [ 2 h* i ] c [ 1 d e ] f g j [ 3 k l m ]",  // Move before 'a'.
      "a b | [ 2 h* i ] c [ 1 d e ] f g j [ 3 k l m ]",  // Move before 'b'.
      "a b | [ 2 h* i ] c [ 1 d e ] f g j [ 3 k l m ]",  // Move before 'c'.
      "a b | c [ 2 h* i ] [ 1 d e ] f g j [ 3 k l m ]",  // Move before 'd'.
      "a b | c [ 2 h* i ] [ 1 d e ] f g j [ 3 k l m ]",  // Move before 'e'.
      "a b | c [ 1 d e ] [ 2 h* i ] f g j [ 3 k l m ]",  // Move before 'f'.
      "a b | c [ 1 d e ] f [ 2 h* i ] g j [ 3 k l m ]",  // Move before 'g'.
      "a b | c [ 1 d e ] f g [ 2 h* i ] j [ 3 k l m ]",  // Move before 'h'.
      "a b | c [ 1 d e ] f g [ 2 h* i ] j [ 3 k l m ]",  // Move before 'i'.
      "a b | c [ 1 d e ] f g [ 2 h* i ] j [ 3 k l m ]",  // Move before 'j'.
      "a b | c [ 1 d e ] f g j [ 2 h* i ] [ 3 k l m ]",  // Move before 'k'.
      "a b | c [ 1 d e ] f g j [ 2 h* i ] [ 3 k l m ]",  // Move before 'l'.
      "a b | c [ 1 d e ] f g j [ 2 h* i ] [ 3 k l m ]",  // Move before 'm'.
      "a b | c [ 1 d e ] f g j [ 3 k l m ] [ 2 h* i ]",  // Move after 'm'.
  };

  int to_index = 0;
  for (const auto& expected_description : expected_description_for_move_index) {
    // Setting up the WebStateList.
    WebStateListBuilderFromDescription builder(&web_state_list_);
    ASSERT_TRUE(builder.BuildWebStateListFromDescription(
        web_state_list_description_before_move));
    observer_.ResetStatistics();
    ASSERT_TRUE(RangesOfTabGroupsAreValid());
    const TabGroup* group_2 = builder.GetTabGroupForIdentifier('2');
    ASSERT_NE(nullptr, group_2);
    const TabGroupRange prior_range = group_2->range();

    // Moving group 2 before `to_index`.
    web_state_list_.MoveGroup(group_2, to_index);

    // Check everything is as expected after the move.
    EXPECT_TRUE(RangesOfTabGroupsAreValid())
        << "\nContiguity of TabGroups broken in WebStateList after move of "
           "group 2."
        << "\nDestination index: " << to_index
        << "\nDescription after move: " << builder.GetWebStateListDescription();
    EXPECT_EQ(expected_description, builder.GetWebStateListDescription());
    EXPECT_EQ(0, observer_.status_only_count());
    EXPECT_EQ(0, observer_.web_state_moved_count());
    if (expected_description == web_state_list_description_before_move) {
      EXPECT_EQ(0, observer_.group_moved_count());
    } else {
      EXPECT_EQ(1, observer_.group_moved_count());
      EXPECT_EQ(group_2, observer_.group_moved_group());
      EXPECT_EQ(prior_range, observer_.group_moved_from_range());
      EXPECT_EQ(group_2->range(), observer_.group_moved_to_range());
    }

    // Resetting.
    CloseAllWebStates(web_state_list_, WebStateList::CLOSE_NO_FLAGS);
    ++to_index;
  }
}

// Tests MoveGroup on a WebStateList when moving a group at different indices.
// The active web state moves, as it's in the moving group.
TEST_F(WebStateListTest, MoveGroup_NotMovingActiveWebState) {
  // Group 2 is moving.
  constexpr std::string_view web_state_list_description_before_move =
      "a b | c [ 1 d e ] f* g [ 2 h i ] j [ 3 k l m ]";
  // Map of whether a GroupMove notification is sent, and the expected
  // description after the move.
  const std::vector<std::string_view> expected_description_for_move_index{
      "a b | [ 2 h i ] c [ 1 d e ] f* g j [ 3 k l m ]",  // Move before 'a'.
      "a b | [ 2 h i ] c [ 1 d e ] f* g j [ 3 k l m ]",  // Move before 'b'.
      "a b | [ 2 h i ] c [ 1 d e ] f* g j [ 3 k l m ]",  // Move before 'c'.
      "a b | c [ 2 h i ] [ 1 d e ] f* g j [ 3 k l m ]",  // Move before 'd'.
      "a b | c [ 2 h i ] [ 1 d e ] f* g j [ 3 k l m ]",  // Move before 'e'.
      "a b | c [ 1 d e ] [ 2 h i ] f* g j [ 3 k l m ]",  // Move before 'f'.
      "a b | c [ 1 d e ] f* [ 2 h i ] g j [ 3 k l m ]",  // Move before 'g'.
      "a b | c [ 1 d e ] f* g [ 2 h i ] j [ 3 k l m ]",  // Move before 'h'.
      "a b | c [ 1 d e ] f* g [ 2 h i ] j [ 3 k l m ]",  // Move before 'i'.
      "a b | c [ 1 d e ] f* g [ 2 h i ] j [ 3 k l m ]",  // Move before 'j'.
      "a b | c [ 1 d e ] f* g j [ 2 h i ] [ 3 k l m ]",  // Move before 'k'.
      "a b | c [ 1 d e ] f* g j [ 2 h i ] [ 3 k l m ]",  // Move before 'l'.
      "a b | c [ 1 d e ] f* g j [ 2 h i ] [ 3 k l m ]",  // Move before 'm'.
      "a b | c [ 1 d e ] f* g j [ 3 k l m ] [ 2 h i ]",  // Move after 'm'.
  };

  int to_index = 0;
  for (const auto& expected_description : expected_description_for_move_index) {
    // Setting up the WebStateList.
    WebStateListBuilderFromDescription builder(&web_state_list_);
    ASSERT_TRUE(builder.BuildWebStateListFromDescription(
        web_state_list_description_before_move));
    observer_.ResetStatistics();
    ASSERT_TRUE(RangesOfTabGroupsAreValid());
    const TabGroup* group_2 = builder.GetTabGroupForIdentifier('2');
    ASSERT_NE(nullptr, group_2);
    const TabGroupRange prior_range = group_2->range();

    // Moving group 2 before `to_index`.
    web_state_list_.MoveGroup(group_2, to_index);

    // Check everything is as expected after the move.
    EXPECT_TRUE(RangesOfTabGroupsAreValid())
        << "\nContiguity of TabGroups broken in WebStateList after move of "
           "group 2."
        << "\nDestination index: " << to_index
        << "\nDescription after move: " << builder.GetWebStateListDescription();
    EXPECT_EQ(expected_description, builder.GetWebStateListDescription());
    if (expected_description == web_state_list_description_before_move) {
      EXPECT_EQ(0, observer_.group_moved_count());
    } else {
      EXPECT_EQ(1, observer_.group_moved_count());
      EXPECT_EQ(group_2, observer_.group_moved_group());
      EXPECT_EQ(prior_range, observer_.group_moved_from_range());
      EXPECT_EQ(group_2->range(), observer_.group_moved_to_range());
    }

    // Resetting.
    CloseAllWebStates(web_state_list_, WebStateList::CLOSE_NO_FLAGS);
    ++to_index;
  }
}

// Tests deleting a group. It keeps the active WebState and doesn’t touch the
// other groups.
TEST_F(WebStateListTest, DeleteGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [0 a* b] [ 1 c ] d"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');

  observer_.ResetStatistics();
  web_state_list_.DeleteGroup(group_0);

  EXPECT_EQ("| a* b [ 1 c ] d", builder.GetWebStateListDescription());
  EXPECT_EQ(TabGroupRange(2, 1), group_1->range());
  EXPECT_EQ(0, observer_.web_state_moved_count());
  EXPECT_EQ(2, observer_.status_only_count());
  EXPECT_FALSE(observer_.web_state_activated());
  EXPECT_EQ(group_0, observer_.status_only_old_group());
  EXPECT_EQ(nullptr, observer_.status_only_new_group());
  EXPECT_EQ(1, observer_.group_deleted_count());
  EXPECT_EQ(group_0, observer_.group_deleted_group());
}

// Tests the check for group membership.
TEST_F(WebStateListTest, ContainsGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [0 a* b] [ 1 c ] d"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');
  TabGroup outside_group{TabGroupId::GenerateNew(),
                         tab_groups::TabGroupVisualData()};

  EXPECT_TRUE(web_state_list_.ContainsGroup(group_0));
  EXPECT_TRUE(web_state_list_.ContainsGroup(group_1));
  EXPECT_FALSE(web_state_list_.ContainsGroup(&outside_group));

  web_state_list_.DeleteGroup(group_1);

  EXPECT_TRUE(web_state_list_.ContainsGroup(group_0));
  EXPECT_FALSE(web_state_list_.ContainsGroup(group_1));
  EXPECT_FALSE(web_state_list_.ContainsGroup(&outside_group));
}

// Tests closing all other web states, with no group and a pinned tab.
TEST_F(WebStateListTest, CloseOtherWebStates_NoGroup) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("a | b c d"));
  observer_.ResetStatistics();
  CloseOtherWebStates(web_state_list_, 2, WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ("a | c", builder.GetWebStateListDescription());
  EXPECT_EQ(2, observer_.web_state_detached_count());
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all other web states, with a group and a pinned tab.
TEST_F(WebStateListTest, CloseOtherWebStates_GroupPinned) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("a | b [ 0 c d ]"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');

  observer_.ResetStatistics();
  CloseOtherWebStates(web_state_list_, 0, WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ("a |", builder.GetWebStateListDescription());
  EXPECT_EQ(3, observer_.web_state_detached_count());
  EXPECT_FALSE(web_state_list_.ContainsGroup(group_0));
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}

// Tests closing all other web states except one in a group, with no pinned tab.
TEST_F(WebStateListTest, CloseOtherWebStates_GroupNoPinned) {
  WebStateListBuilderFromDescription builder(&web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a b [ 0 c d ]"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');

  observer_.ResetStatistics();
  CloseOtherWebStates(web_state_list_, 3, WebStateList::CLOSE_USER_ACTION);

  EXPECT_EQ("| [ 0 d ]", builder.GetWebStateListDescription());
  EXPECT_EQ(3, observer_.web_state_detached_count());
  EXPECT_TRUE(web_state_list_.ContainsGroup(group_0));
  EXPECT_TRUE(observer_.batch_operation_started());
  EXPECT_TRUE(observer_.batch_operation_ended());
}
