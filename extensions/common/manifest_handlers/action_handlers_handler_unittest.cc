// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/test/values_test_util.h"
#include "components/version_info/version_info.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/action_handlers_handler.h"
#include "extensions/common/manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace app_runtime = api::app_runtime;

namespace {

class ActionHandlersManifestTest : public ManifestTest {
 protected:
  ManifestData CreateManifest(const std::string& action_handlers) {
    base::Value manifest = base::test::ParseJson(R"json({
                                    "name": "test",
                                    "version": "1",
                                    "app": {
                                      "background": {
                                        "scripts": ["background.js"]
                                      }
                                    },
                                    "manifest_version": 2,
                                    "action_handlers": )json" +
                                                 action_handlers + "}");
    return ManifestData(std::move(manifest).TakeDict());
  }

  // Returns all action handlers associated with |extension|.
  std::set<app_runtime::ActionType> GetActionHandlers(
      const Extension* extension) {
    ActionHandlersInfo* info = static_cast<ActionHandlersInfo*>(
        extension->GetManifestData(manifest_keys::kActionHandlers));
    return info ? info->action_handlers : std::set<app_runtime::ActionType>();
  }
};

}  // namespace

TEST_F(ActionHandlersManifestTest, InvalidType) {
  LoadAndExpectError(CreateManifest("32"),
                     manifest_errors::kInvalidActionHandlersType);
  LoadAndExpectError(CreateManifest("[true]"),
                     manifest_errors::kInvalidActionHandlersType);
  LoadAndExpectError(CreateManifest(R"(["invalid_handler"])"),
                     manifest_errors::kInvalidActionHandlersActionType);
  LoadAndExpectError(CreateManifest(R"(["invalid_handler"])"),
                     manifest_errors::kInvalidActionHandlersActionType);
  LoadAndExpectError(CreateManifest("[{}]"),
                     manifest_errors::kInvalidActionHandlerDictionary);
  LoadAndExpectError(CreateManifest(R"([{"action": "invalid_handler"}])"),
                     manifest_errors::kInvalidActionHandlersActionType);
}

TEST_F(ActionHandlersManifestTest, VerifyParse) {
  scoped_refptr<Extension> none = LoadAndExpectSuccess(CreateManifest("[]"));
  EXPECT_TRUE(GetActionHandlers(none.get()).empty());

  EXPECT_FALSE(ActionHandlersInfo::HasActionHandler(
      none.get(), app_runtime::ActionType::kNewNote));

  scoped_refptr<Extension> new_note =
      LoadAndExpectSuccess(CreateManifest("[\"new_note\"]"));
  EXPECT_EQ(
      std::set<app_runtime::ActionType>{app_runtime::ActionType::kNewNote},
      GetActionHandlers(new_note.get()));
  EXPECT_TRUE(ActionHandlersInfo::HasActionHandler(
      new_note.get(), app_runtime::ActionType::kNewNote));
}

TEST_F(ActionHandlersManifestTest, ParseDictionaryActionValues) {
  scoped_refptr<Extension> new_note_key =
      LoadAndExpectSuccess(CreateManifest(R"([{"action": "new_note"}])"));
  EXPECT_EQ(
      std::set<app_runtime::ActionType>{app_runtime::ActionType::kNewNote},
      GetActionHandlers(new_note_key.get()));
  EXPECT_TRUE(ActionHandlersInfo::HasActionHandler(
      new_note_key.get(), app_runtime::ActionType::kNewNote));
  scoped_refptr<Extension> no_new_note_key =
      LoadAndExpectSuccess(CreateManifest(R"([])"));
  EXPECT_TRUE(GetActionHandlers(no_new_note_key.get()).empty());
}

TEST_F(ActionHandlersManifestTest, DuplicateHandlers) {
  LoadAndExpectError(CreateManifest(R"(["new_note", {"action": "new_note"}])"),
                     manifest_errors::kDuplicateActionHandlerFound);
  LoadAndExpectError(CreateManifest(
                         R"(["new_note", {
                              "action": "new_note",
                            }])"),
                     manifest_errors::kDuplicateActionHandlerFound);
  LoadAndExpectError(CreateManifest(
                         R"([{
                              "action": "new_note"
                            }, {
                              "action": "new_note",
                            }])"),
                     manifest_errors::kDuplicateActionHandlerFound);
}

}  // namespace extensions
