// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/extension_pref_value_map.h"

#include <stdint.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_store_observer_mock.h"
#include "extensions/common/api/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kExt1[] = "ext1";
const char kExt2[] = "ext2";
const char kExt3[] = "ext3";

const char kPref1[] = "path1.subpath";
const char kPref2[] = "path2";
const char kPref3[] = "path3";
const char kPref4[] = "path4";
}  // namespace

static base::Value CreateVal(const char* str) {
  return base::Value(str);
}

static base::Time CreateTime(int64_t t) {
  return base::Time::FromInternalValue(t);
}

template <typename BASECLASS>
class ExtensionPrefValueMapTestBase : public BASECLASS {
 public:
  using ChromeSettingScope = extensions::api::types::ChromeSettingScope;

  // Returns an empty string if the key is not set.
  std::string GetValue(const char * key, bool incognito) const {
    const base::Value* value =
        epvm_.GetEffectivePrefValue(key, incognito, nullptr);
    return value && value->is_string() ? value->GetString() : std::string();
  }

  // Registers the extension as enabled but without incognito permission.
  void RegisterExtension(const std::string& ext_id,
                         const base::Time& install_time) {
    epvm_.RegisterExtension(
        ext_id, install_time, true /*enabled*/, false /*incognito*/);
  }

 protected:
  ExtensionPrefValueMap epvm_;
};

class ExtensionPrefValueMapTest
    : public ExtensionPrefValueMapTestBase<testing::Test> {
};

// A gmock-ified implementation of PrefStore::Observer.
class ExtensionPrefValueMapObserverMock
    : public ExtensionPrefValueMap::Observer {
 public:
  ExtensionPrefValueMapObserverMock() {}

  ExtensionPrefValueMapObserverMock(const ExtensionPrefValueMapObserverMock&) =
      delete;
  ExtensionPrefValueMapObserverMock& operator=(
      const ExtensionPrefValueMapObserverMock&) = delete;

  ~ExtensionPrefValueMapObserverMock() override {}

  MOCK_METHOD1(OnPrefValueChanged, void(const std::string&));
  MOCK_METHOD0(OnInitializationCompleted, void());
  MOCK_METHOD0(OnExtensionPrefValueMapDestruction, void());
};

TEST_F(ExtensionPrefValueMapTest, SetAndGetPrefValue) {
  RegisterExtension(kExt1, CreateTime(10));
  epvm_.SetExtensionPref(kExt1, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val1"));
  EXPECT_EQ("val1", GetValue(kPref1, false));
}

TEST_F(ExtensionPrefValueMapTest, GetNotSetPrefValue) {
  RegisterExtension(kExt1, CreateTime(10));
  EXPECT_EQ(std::string(), GetValue(kPref1, false));
}

// Make sure the last-installed extension wins for each preference.
TEST_F(ExtensionPrefValueMapTest, Override) {
  RegisterExtension(kExt1, CreateTime(10));
  RegisterExtension(kExt2, CreateTime(20));
  RegisterExtension(kExt3, CreateTime(30));

  epvm_.SetExtensionPref(kExt1, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val1"));
  epvm_.SetExtensionPref(kExt2, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val2"));
  epvm_.SetExtensionPref(kExt3, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val3"));

  epvm_.SetExtensionPref(kExt1, kPref2, ChromeSettingScope::kRegular,
                         CreateVal("val4"));
  epvm_.SetExtensionPref(kExt2, kPref2, ChromeSettingScope::kRegular,
                         CreateVal("val5"));

  epvm_.SetExtensionPref(kExt1, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val6"));
  epvm_.SetExtensionPref(kExt1, kPref2, ChromeSettingScope::kRegular,
                         CreateVal("val7"));
  epvm_.SetExtensionPref(kExt1, kPref3, ChromeSettingScope::kRegular,
                         CreateVal("val8"));

  EXPECT_EQ("val3", GetValue(kPref1, false));
  EXPECT_EQ("val5", GetValue(kPref2, false));
  EXPECT_EQ("val8", GetValue(kPref3, false));
}

TEST_F(ExtensionPrefValueMapTest, OverrideChecks) {
  RegisterExtension(kExt1, CreateTime(10));
  RegisterExtension(kExt2, CreateTime(20));
  RegisterExtension(kExt3, CreateTime(30));

  EXPECT_FALSE(epvm_.DoesExtensionControlPref(kExt1, kPref1, nullptr));
  EXPECT_FALSE(epvm_.DoesExtensionControlPref(kExt2, kPref1, nullptr));
  EXPECT_FALSE(epvm_.DoesExtensionControlPref(kExt3, kPref1, nullptr));
  EXPECT_TRUE(epvm_.CanExtensionControlPref(kExt1, kPref1, false));
  EXPECT_TRUE(epvm_.CanExtensionControlPref(kExt2, kPref1, false));
  EXPECT_TRUE(epvm_.CanExtensionControlPref(kExt3, kPref1, false));

  epvm_.SetExtensionPref(kExt2, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val1"));

  EXPECT_FALSE(epvm_.DoesExtensionControlPref(kExt1, kPref1, nullptr));
  EXPECT_TRUE(epvm_.DoesExtensionControlPref(kExt2, kPref1, nullptr));
  EXPECT_FALSE(epvm_.DoesExtensionControlPref(kExt3, kPref1, nullptr));
  EXPECT_FALSE(epvm_.CanExtensionControlPref(kExt1, kPref1, false));
  EXPECT_TRUE(epvm_.CanExtensionControlPref(kExt2, kPref1, false));
  EXPECT_TRUE(epvm_.CanExtensionControlPref(kExt3, kPref1, false));
}

TEST_F(ExtensionPrefValueMapTest, SetAndGetPrefValueIncognito) {
  RegisterExtension(kExt1, CreateTime(10));
  epvm_.SetExtensionPref(kExt1, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val1"));
  // Check that the value is not propagated until the extension gets incognito
  // permission.
  EXPECT_EQ(std::string(), GetValue(kPref1, true));
  epvm_.SetExtensionIncognitoState(kExt1, true);
  EXPECT_EQ("val1", GetValue(kPref1, true));
  epvm_.SetExtensionIncognitoState(kExt1, false);
  EXPECT_EQ(std::string(), GetValue(kPref1, true));
}

TEST_F(ExtensionPrefValueMapTest, UninstallOnlyExtension) {
  RegisterExtension(kExt1, CreateTime(10));
  epvm_.SetExtensionPref(kExt1, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val1"));
  epvm_.UnregisterExtension(kExt1);

  EXPECT_EQ(std::string(), GetValue(kPref1, false));
}

// Tests uninstalling an extension that wasn't winning for any preferences.
TEST_F(ExtensionPrefValueMapTest, UninstallIrrelevantExtension) {
  RegisterExtension(kExt1, CreateTime(10));
  RegisterExtension(kExt2, CreateTime(10));

  epvm_.SetExtensionPref(kExt1, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val1"));
  epvm_.SetExtensionPref(kExt2, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val2"));

  epvm_.SetExtensionPref(kExt1, kPref2, ChromeSettingScope::kRegular,
                         CreateVal("val3"));
  epvm_.SetExtensionPref(kExt2, kPref2, ChromeSettingScope::kRegular,
                         CreateVal("val4"));

  epvm_.UnregisterExtension(kExt1);

  EXPECT_EQ("val2", GetValue(kPref1, false));
  EXPECT_EQ("val4", GetValue(kPref2, false));
}

// Tests uninstalling an extension that was winning for all preferences.
TEST_F(ExtensionPrefValueMapTest, UninstallExtensionFromTop) {
  RegisterExtension(kExt1, CreateTime(10));
  RegisterExtension(kExt2, CreateTime(20));
  RegisterExtension(kExt3, CreateTime(30));

  epvm_.SetExtensionPref(kExt1, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val1"));
  epvm_.SetExtensionPref(kExt2, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val2"));
  epvm_.SetExtensionPref(kExt3, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val3"));

  epvm_.SetExtensionPref(kExt1, kPref2, ChromeSettingScope::kRegular,
                         CreateVal("val4"));
  epvm_.SetExtensionPref(kExt3, kPref2, ChromeSettingScope::kRegular,
                         CreateVal("val5"));

  epvm_.UnregisterExtension(kExt3);

  EXPECT_EQ("val2", GetValue(kPref1, false));
  EXPECT_EQ("val4", GetValue(kPref2, false));
}

// Tests uninstalling an extension that was winning for only some preferences.
TEST_F(ExtensionPrefValueMapTest, UninstallExtensionFromMiddle) {
  RegisterExtension(kExt1, CreateTime(10));
  RegisterExtension(kExt2, CreateTime(20));
  RegisterExtension(kExt3, CreateTime(30));

  epvm_.SetExtensionPref(kExt1, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val1"));
  epvm_.SetExtensionPref(kExt2, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val2"));
  epvm_.SetExtensionPref(kExt3, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val3"));

  epvm_.SetExtensionPref(kExt1, kPref2, ChromeSettingScope::kRegular,
                         CreateVal("val4"));
  epvm_.SetExtensionPref(kExt2, kPref2, ChromeSettingScope::kRegular,
                         CreateVal("val5"));

  epvm_.SetExtensionPref(kExt1, kPref3, ChromeSettingScope::kRegular,
                         CreateVal("val6"));

  epvm_.SetExtensionPref(kExt2, kPref4, ChromeSettingScope::kRegular,
                         CreateVal("val7"));

  epvm_.UnregisterExtension(kExt2);

  EXPECT_EQ("val3", GetValue(kPref1, false));
  EXPECT_EQ("val4", GetValue(kPref2, false));
  EXPECT_EQ("val6", GetValue(kPref3, false));
  EXPECT_EQ(std::string(), GetValue(kPref4, false));
}

// Tests triggering of notifications to registered observers.
TEST_F(ExtensionPrefValueMapTest, NotifyWhenNeeded) {
  using testing::Mock;
  using testing::StrEq;

  RegisterExtension(kExt1, CreateTime(10));

  ExtensionPrefValueMapObserverMock observer;
  epvm_.AddObserver(&observer);

  EXPECT_CALL(observer, OnPrefValueChanged(std::string(kPref1)));
  epvm_.SetExtensionPref(kExt1, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val1"));
  Mock::VerifyAndClearExpectations(&observer);

  // Write the same value again.
  EXPECT_CALL(observer, OnPrefValueChanged(std::string(kPref1))).Times(0);
  epvm_.SetExtensionPref(kExt1, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val1"));
  Mock::VerifyAndClearExpectations(&observer);

  // Override incognito value.
  EXPECT_CALL(observer, OnPrefValueChanged(std::string(kPref1)));
  epvm_.SetExtensionPref(kExt1, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val2"));
  Mock::VerifyAndClearExpectations(&observer);

  // Override non-incognito value.
  EXPECT_CALL(observer, OnPrefValueChanged(std::string(kPref1)));
  epvm_.SetExtensionPref(kExt1, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val3"));
  Mock::VerifyAndClearExpectations(&observer);

  // Disable.
  EXPECT_CALL(observer, OnPrefValueChanged(std::string(kPref1)));
  epvm_.SetExtensionState(kExt1, false);
  Mock::VerifyAndClearExpectations(&observer);

  // Enable.
  EXPECT_CALL(observer, OnPrefValueChanged(std::string(kPref1)));
  epvm_.SetExtensionState(kExt1, true);
  Mock::VerifyAndClearExpectations(&observer);

  // Uninstall
  EXPECT_CALL(observer, OnPrefValueChanged(std::string(kPref1)));
  epvm_.UnregisterExtension(kExt1);
  Mock::VerifyAndClearExpectations(&observer);

  epvm_.RemoveObserver(&observer);

  // Write new value --> no notification after removing observer.
  EXPECT_CALL(observer, OnPrefValueChanged(std::string(kPref1))).Times(0);
  RegisterExtension(kExt1, CreateTime(10));
  epvm_.SetExtensionPref(kExt1, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val4"));
  Mock::VerifyAndClearExpectations(&observer);
}

// Tests disabling an extension.
TEST_F(ExtensionPrefValueMapTest, DisableExt) {
  RegisterExtension(kExt1, CreateTime(10));

  epvm_.SetExtensionPref(kExt1, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val1"));
  epvm_.SetExtensionState(kExt1, false);
  EXPECT_EQ(std::string(), GetValue(kPref1, false));
}

// Tests disabling and reenabling an extension.
TEST_F(ExtensionPrefValueMapTest, ReenableExt) {
  RegisterExtension(kExt1, CreateTime(10));

  epvm_.SetExtensionPref(kExt1, kPref1, ChromeSettingScope::kRegular,
                         CreateVal("val1"));
  epvm_.SetExtensionState(kExt1, false);
  epvm_.SetExtensionState(kExt1, true);
  EXPECT_EQ("val1", GetValue(kPref1, false));
}

struct OverrideIncognitoTestCase {
  OverrideIncognitoTestCase(bool enable_ext1_in_incognito,
                            bool enable_ext2_in_incognito,
                            int val_ext1_regular,
                            int val_ext1_regular_only,
                            int val_ext1_incognito_pers,
                            int val_ext1_incognito_sess,
                            int val_ext2_regular,
                            int val_ext2_regular_only,
                            int val_ext2_incognito_pers,
                            int val_ext2_incognito_sess,
                            int effective_value_regular,
                            int effective_value_incognito)
      : enable_ext1_in_incognito_(enable_ext1_in_incognito),
        enable_ext2_in_incognito_(enable_ext2_in_incognito),
        val_ext1_regular_(val_ext1_regular),
        val_ext1_regular_only_(val_ext1_regular_only),
        val_ext1_incognito_pers_(val_ext1_incognito_pers),
        val_ext1_incognito_sess_(val_ext1_incognito_sess),
        val_ext2_regular_(val_ext2_regular),
        val_ext2_regular_only_(val_ext2_regular_only),
        val_ext2_incognito_pers_(val_ext2_incognito_pers),
        val_ext2_incognito_sess_(val_ext2_incognito_sess),
        effective_value_regular_(effective_value_regular),
        effective_value_incognito_(effective_value_incognito) {}

  bool enable_ext1_in_incognito_;
  bool enable_ext2_in_incognito_;

  // pers. = persistent
  // sess. = session only
  int val_ext1_regular_;           // pref value of extension 1
  int val_ext1_regular_only_;      // pref value of extension 1 regular-only.
  int val_ext1_incognito_pers_;    // pref value of extension 1 incognito pers.
  int val_ext1_incognito_sess_;    // pref value of extension 1 incognito sess.
  int val_ext2_regular_;           // pref value of extension 2
  int val_ext2_regular_only_;      // pref value of extension 2 regular-only.
  int val_ext2_incognito_pers_;    // pref value of extension 2 incognito pers.
  int val_ext2_incognito_sess_;    // pref value of extension 2 incognito sess.
  int effective_value_regular_;    // desired winner regular
  int effective_value_incognito_;  // desired winner incognito
};

class ExtensionPrefValueMapTestIncognitoTests
    : public ExtensionPrefValueMapTestBase<
          testing::TestWithParam<OverrideIncognitoTestCase> > {
};

TEST_P(ExtensionPrefValueMapTestIncognitoTests, OverrideIncognito) {
  OverrideIncognitoTestCase test = GetParam();
  const char* strings[] = {
      "",  // undefined
      "val1",
      "val2",
      "val3",
      "val4",
      "val5",
      "val6",
      "val7",
      "val8",
  };

  const bool kEnabled = true;
  epvm_.RegisterExtension(
      kExt1, CreateTime(10), kEnabled, test.enable_ext1_in_incognito_);
  epvm_.RegisterExtension(
      kExt2, CreateTime(20), kEnabled, test.enable_ext2_in_incognito_);
  if (test.val_ext1_regular_) {
    epvm_.SetExtensionPref(kExt1, kPref1, ChromeSettingScope::kRegular,
                           CreateVal(strings[test.val_ext1_regular_]));
  }
  if (test.val_ext1_regular_only_) {
    epvm_.SetExtensionPref(kExt1, kPref1, ChromeSettingScope::kRegularOnly,
                           CreateVal(strings[test.val_ext1_regular_only_]));
  }
  if (test.val_ext1_incognito_pers_) {
    epvm_.SetExtensionPref(kExt1, kPref1,
                           ChromeSettingScope::kIncognitoPersistent,
                           CreateVal(strings[test.val_ext1_incognito_pers_]));
  }
  if (test.val_ext1_incognito_sess_) {
    epvm_.SetExtensionPref(kExt1, kPref1,
                           ChromeSettingScope::kIncognitoSessionOnly,
                           CreateVal(strings[test.val_ext1_incognito_sess_]));
  }
  if (test.val_ext2_regular_) {
    epvm_.SetExtensionPref(kExt2, kPref1, ChromeSettingScope::kRegular,
                           CreateVal(strings[test.val_ext2_regular_]));
  }
  if (test.val_ext2_regular_only_) {
    epvm_.SetExtensionPref(kExt2, kPref1, ChromeSettingScope::kRegularOnly,
                           CreateVal(strings[test.val_ext2_regular_only_]));
  }
  if (test.val_ext2_incognito_pers_) {
    epvm_.SetExtensionPref(kExt2, kPref1,
                           ChromeSettingScope::kIncognitoPersistent,
                           CreateVal(strings[test.val_ext2_incognito_pers_]));
  }
  if (test.val_ext2_incognito_sess_) {
    epvm_.SetExtensionPref(kExt2, kPref1,
                           ChromeSettingScope::kIncognitoSessionOnly,
                           CreateVal(strings[test.val_ext2_incognito_sess_]));
  }
  std::string actual;
  EXPECT_EQ(strings[test.effective_value_regular_], GetValue(kPref1, false));
  EXPECT_EQ(strings[test.effective_value_incognito_], GetValue(kPref1, true));
  epvm_.UnregisterExtension(kExt1);
  epvm_.UnregisterExtension(kExt2);
}

INSTANTIATE_TEST_SUITE_P(
    ExtensionPrefValueMapTestIncognitoTestsInstance,
    ExtensionPrefValueMapTestIncognitoTests,
    testing::Values(
        // e.g. (true, 1, 0, 0, 0,  0, 0, 7, 0,  1, 7), means:
        // ext1 regular is set to "val1", ext2 incognito persistent is set to
        // "val7"
        // --> the winning regular value is "val1", the winning incognito
        //     value is "val7".
        OverrideIncognitoTestCase(true, true, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1),
        OverrideIncognitoTestCase(true, true, 1, 2, 0, 0, 0, 0, 0, 0, 2, 1),
        OverrideIncognitoTestCase(true, true, 1, 0, 3, 0, 0, 0, 0, 0, 1, 3),
        OverrideIncognitoTestCase(true, true, 1, 0, 0, 4, 0, 0, 0, 0, 1, 4),
        OverrideIncognitoTestCase(true, true, 1, 0, 3, 4, 0, 0, 0, 0, 1, 4),
        OverrideIncognitoTestCase(true, true, 1, 2, 3, 0, 0, 0, 0, 0, 2, 3),
        OverrideIncognitoTestCase(true, true, 1, 0, 0, 0, 5, 0, 0, 0, 5, 5),
        OverrideIncognitoTestCase(true, true, 1, 2, 3, 0, 5, 0, 0, 0, 5, 5),
        OverrideIncognitoTestCase(true, true, 1, 0, 0, 0, 0, 6, 0, 0, 6, 1),
        OverrideIncognitoTestCase(true, true, 1, 0, 3, 0, 5, 6, 0, 0, 6, 5),
        OverrideIncognitoTestCase(true, true, 1, 0, 0, 4, 5, 6, 0, 0, 6, 5),
        OverrideIncognitoTestCase(true, true, 1, 0, 0, 0, 0, 0, 7, 0, 1, 7),
        OverrideIncognitoTestCase(true, true, 1, 2, 0, 0, 5, 0, 7, 0, 5, 7),
        OverrideIncognitoTestCase(true, true, 1, 2, 0, 0, 5, 0, 0, 8, 5, 8),
        OverrideIncognitoTestCase(true, true, 1, 2, 0, 0, 5, 0, 7, 8, 5, 8),
        OverrideIncognitoTestCase(true, true, 1, 2, 3, 0, 0, 6, 7, 0, 6, 7),
        // Same tests as above but w/o incognito permission.
        OverrideIncognitoTestCase(false, false, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0),
        OverrideIncognitoTestCase(false, false, 1, 2, 0, 0, 0, 0, 0, 0, 2, 0),
        OverrideIncognitoTestCase(false, false, 1, 0, 3, 0, 0, 0, 0, 0, 1, 0),
        OverrideIncognitoTestCase(false, false, 1, 0, 0, 4, 0, 0, 0, 0, 1, 0),
        OverrideIncognitoTestCase(false, false, 1, 0, 3, 4, 0, 0, 0, 0, 1, 0),
        OverrideIncognitoTestCase(false, false, 1, 2, 3, 0, 0, 0, 0, 0, 2, 0),
        OverrideIncognitoTestCase(false, false, 1, 0, 0, 0, 5, 0, 0, 0, 5, 0),
        OverrideIncognitoTestCase(false, false, 1, 2, 3, 0, 5, 0, 0, 0, 5, 0),
        OverrideIncognitoTestCase(false, false, 1, 0, 0, 0, 0, 6, 0, 0, 6, 0),
        OverrideIncognitoTestCase(false, false, 1, 0, 3, 0, 5, 6, 0, 0, 6, 0),
        OverrideIncognitoTestCase(false, false, 1, 0, 0, 4, 5, 6, 0, 0, 6, 0),
        OverrideIncognitoTestCase(false, false, 1, 0, 0, 0, 0, 0, 7, 0, 1, 0),
        OverrideIncognitoTestCase(false, false, 1, 2, 0, 0, 5, 0, 7, 0, 5, 0),
        OverrideIncognitoTestCase(false, false, 1, 2, 0, 0, 5, 0, 0, 8, 5, 0),
        OverrideIncognitoTestCase(false, false, 1, 2, 0, 0, 5, 0, 7, 8, 5, 0),
        OverrideIncognitoTestCase(false, false, 1, 2, 3, 0, 0, 6, 7, 0, 6, 0)));
