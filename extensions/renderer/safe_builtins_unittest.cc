// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/module_system_test.h"

namespace extensions {
namespace {

class SafeBuiltinsUnittest : public ModuleSystemTest {};

TEST_F(SafeBuiltinsUnittest, TestNotOriginalObject) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("test",
                        "var assert = requireNative('assert');\n"
                        "Array.foo = 10;\n"
                        "assert.AssertTrue(!$Array.hasOwnProperty('foo'));\n");
  env()->module_system()->Require("test");
}

TEST_F(SafeBuiltinsUnittest, TestSelf) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("test",
                        "var assert = requireNative('assert');\n"
                        "Array.foo = 10;\n"
                        "assert.AssertTrue($Array.self.foo == 10);\n"
                        "var arr = $Array.self(1);\n"
                        "assert.AssertTrue(arr.length == 1);\n"
                        "assert.AssertTrue(arr[0] === undefined);\n");
  env()->module_system()->Require("test");
}

TEST_F(SafeBuiltinsUnittest, TestStaticFunction) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("test",
                        "var assert = requireNative('assert');\n"
                        "Object.keys = function() {throw new Error()};\n"
                        "var obj = {a: 10};\n"
                        "var keys = $Object.keys(obj);\n"
                        "assert.AssertTrue(keys.length == 1);\n"
                        "assert.AssertTrue(keys[0] == 'a');\n");
  env()->module_system()->Require("test");
}

TEST_F(SafeBuiltinsUnittest, TestInstanceMethod) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule(
      "test",
      "var assert = requireNative('assert');\n"
      "Array.prototype.push = function() {throw new Error();}\n"
      "var arr = []\n"
      "$Array.push(arr, 1);\n"
      "assert.AssertTrue(arr.length == 1);\n"
      "assert.AssertTrue(arr[0] == 1);\n");
  env()->module_system()->Require("test");
}

// NOTE: JSON is already tested in ExtensionApiTest.Messaging, via
// chrome/test/data/extensions/api_test/messaging/connect/page.js.

}  // namespace
}  // namespace extensions
