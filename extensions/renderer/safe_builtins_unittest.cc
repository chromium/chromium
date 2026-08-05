// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/safe_builtins.h"

#include "extensions/renderer/module_system_test.h"
#include "extensions/renderer/script_context.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-script.h"

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
  env()->RegisterModule("test", R"(
        var assert = requireNative('assert');
        Array.prototype.push = function() { throw new Error(); };
        var arr = [];
        $Array.push(arr, 1);
        assert.AssertTrue(arr.length == 1);
        assert.AssertTrue(arr[0] == 1);
      )");
  env()->module_system()->Require("test");
}

TEST_F(SafeBuiltinsUnittest, TestClobberingBeforeSafeBuiltinAccess) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());

  // Clobber builtins in JS global context before accessing safe builtins.
  v8::Local<v8::Context> v8_context = env()->context()->v8_context();
  v8::Context::Scope context_scope(v8_context);
  v8::Isolate* isolate = env()->isolate();
  const char* clobber_script = R"(
    Array.prototype.push = function() { throw new Error('clobbered'); };
    Array.prototype.forEach = function() { throw new Error('clobbered'); };
    Array.isArray = function() { throw new Error('clobbered'); };
    Object.keys = function() { throw new Error('clobbered'); };
    Function.prototype.apply = function() { throw new Error('clobbered'); };
    String.prototype.indexOf = function() { throw new Error('clobbered'); };
    RegExp.prototype.exec = function() { throw new Error('clobbered'); };
    JSON.stringify = function() { throw new Error('clobbered'); };
  )";
  v8::Script::Compile(
      v8_context,
      v8::String::NewFromUtf8(isolate, clobber_script).ToLocalChecked())
      .ToLocalChecked()
      ->Run(v8_context)
      .ToLocalChecked();

  env()->RegisterModule("test", R"(
        var assert = requireNative('assert');
        var arr = [];
        $Array.push(arr, 42);
        assert.AssertTrue(arr.length === 1 && arr[0] === 42);
        assert.AssertTrue($Array.isArray(arr));
        var keys = $Object.keys({a: 1});
        assert.AssertTrue(keys.length === 1 && keys[0] === 'a');
        var fn = function(x) { return x + 1; };
        assert.AssertTrue($Function.apply(fn, null, [5]) === 6);
        assert.AssertTrue($String.indexOf('hello', 'e') === 1);
        var match = $RegExp.exec(/a(b)/, 'ab');
        assert.AssertTrue(match !== null && match[1] === 'b');
        assert.AssertTrue($JSON.stringify({a: 1}) === '{"a":1}');
      )");
  env()->module_system()->Require("test");
}

TEST_F(SafeBuiltinsUnittest, TestAllSafeBuiltinsExposed) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("test", R"(
        var assert = requireNative('assert');
        function check(safeObj, realObj, methods, staticMethods) {
          assert.AssertTrue(safeObj.self === realObj);
          if (methods) {
            methods.forEach(function(m) {
              assert.AssertTrue(typeof safeObj[m] === 'function');
            });
          }
          if (staticMethods) {
            staticMethods.forEach(function(m) {
              assert.AssertTrue(typeof safeObj[m] === 'function');
            });
          }
        }

        check($Array, Array,
              ['concat', 'forEach', 'includes', 'indexOf', 'join',
               'push', 'slice', 'splice', 'map', 'filter', 'shift',
               'unshift', 'pop', 'reverse', 'find'],
              ['from', 'isArray']);

        check($Function, Function, ['apply', 'bind', 'call']);

        check($Object, Object, ['hasOwnProperty'],
              ['assign', 'create', 'defineProperty', 'entries',
               'freeze', 'getOwnPropertyDescriptor',
               'getPrototypeOf', 'keys', 'setPrototypeOf']);

        check($String, String,
              ['indexOf', 'slice', 'split', 'substr',
               'toLowerCase', 'toUpperCase', 'replace'],
              ['fromCharCode']);

        check($RegExp, RegExp, ['exec']);

        check($Error, Error, null, ['captureStackTrace']);

        check($Promise, Promise, ['then', 'catch'], ['race', 'resolve']);

        assert.AssertTrue($Symbol.toStringTag === Symbol.toStringTag);
        assert.AssertTrue(typeof $JSON.parse === 'function');
        assert.AssertTrue(typeof $JSON.stringify === 'function');
      )");
  env()->module_system()->Require("test");
}

TEST_F(SafeBuiltinsUnittest, TestCannotCallOrConstruct) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("test", R"(
        var assert = requireNative('assert');
        var check = function(fn) {
          try {
            fn();
            return false;
          } catch (e) {
            return true;
          }
        };
        assert.AssertTrue(check(function() { $Array(); }));
        assert.AssertTrue(check(function() { new $Array(); }));
        assert.AssertTrue(check(function() { $Object(); }));
        assert.AssertTrue(check(function() { new $Object(); }));
        assert.AssertTrue(check(function() { $Function(); }));
        assert.AssertTrue(check(function() { new $Function(); }));
        assert.AssertTrue(check(function() { $String(); }));
        assert.AssertTrue(check(function() { new $String(); }));
        assert.AssertTrue(check(function() { $RegExp(); }));
        assert.AssertTrue(check(function() { new $RegExp(); }));
        assert.AssertTrue(check(function() { $Error(); }));
        assert.AssertTrue(check(function() { new $Error(); }));
        assert.AssertTrue(check(function() { $Promise(); }));
        assert.AssertTrue(check(function() { new $Promise(); }));
      )");
  env()->module_system()->Require("test");
}

TEST_F(SafeBuiltinsUnittest, TestLazyJSObjectConstructionAndCaching) {
  ExpectNoAssertionsMade();
  ScriptContext* context = env()->context();
  SafeBuiltins* safe_builtins = context->safe_builtins();

  v8::Local<v8::Object> array1 = safe_builtins->GetArray();
  v8::Local<v8::Object> array2 = safe_builtins->GetArray();
  EXPECT_FALSE(array1.IsEmpty());
  EXPECT_EQ(array1, array2);

  v8::Local<v8::Object> function1 = safe_builtins->GetFunction();
  v8::Local<v8::Object> function2 = safe_builtins->GetFunction();
  EXPECT_FALSE(function1.IsEmpty());
  EXPECT_EQ(function1, function2);

  v8::Local<v8::Object> json1 = safe_builtins->GetJSON();
  v8::Local<v8::Object> json2 = safe_builtins->GetJSON();
  EXPECT_FALSE(json1.IsEmpty());
  EXPECT_EQ(json1, json2);

  v8::Local<v8::Object> object1 = safe_builtins->GetObjekt();
  v8::Local<v8::Object> object2 = safe_builtins->GetObjekt();
  EXPECT_FALSE(object1.IsEmpty());
  EXPECT_EQ(object1, object2);

  v8::Local<v8::Object> regexp1 = safe_builtins->GetRegExp();
  v8::Local<v8::Object> regexp2 = safe_builtins->GetRegExp();
  EXPECT_FALSE(regexp1.IsEmpty());
  EXPECT_EQ(regexp1, regexp2);

  v8::Local<v8::Object> string1 = safe_builtins->GetString();
  v8::Local<v8::Object> string2 = safe_builtins->GetString();
  EXPECT_FALSE(string1.IsEmpty());
  EXPECT_EQ(string1, string2);

  v8::Local<v8::Object> error1 = safe_builtins->GetError();
  v8::Local<v8::Object> error2 = safe_builtins->GetError();
  EXPECT_FALSE(error1.IsEmpty());
  EXPECT_EQ(error1, error2);

  v8::Local<v8::Object> promise1 = safe_builtins->GetPromise();
  v8::Local<v8::Object> promise2 = safe_builtins->GetPromise();
  EXPECT_FALSE(promise1.IsEmpty());
  EXPECT_EQ(promise1, promise2);

  v8::Local<v8::Object> symbol1 = safe_builtins->GetSymbol();
  v8::Local<v8::Object> symbol2 = safe_builtins->GetSymbol();
  EXPECT_FALSE(symbol1.IsEmpty());
  EXPECT_EQ(symbol1, symbol2);
}

TEST_F(SafeBuiltinsUnittest, TestInstanceMethodReceiverHandling) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("test", R"(
        var assert = requireNative('assert');
        // Calling instance method with no receiver (0 args) throws TypeError.
        var threw = false;
        try {
          $Array.push();
        } catch (e) {
          threw = true;
        }
        assert.AssertTrue(threw);

        // Receiver passed as first argument.
        var arr = ['a', 'b', 'c'];
        assert.AssertTrue($Array.indexOf(arr, 'b') === 1);
        var sliced = $Array.slice(arr, 1, 3);
        assert.AssertTrue(
            sliced.length === 2 &&
            sliced[0] === 'b' &&
            sliced[1] === 'c');

        // Primitive receiver for string instance methods.
        assert.AssertTrue($String.slice('hello world', 0, 5) === 'hello');
        assert.AssertTrue($String.toUpperCase('abc') === 'ABC');

        // Exception on incompatible receiver.
        threw = false;
        try {
          $RegExp.exec({}, 'abc');
        } catch (e) {
          threw = true;
        }
        assert.AssertTrue(threw);
      )");
  env()->module_system()->Require("test");
}

TEST_F(SafeBuiltinsUnittest, TestStaticMethodInvocation) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("test", R"(
        var assert = requireNative('assert');

        // Object static methods.
        var target = { a: 1 };
        $Object.assign(target, { b: 2 });
        assert.AssertTrue(target.a === 1 && target.b === 2);
        assert.AssertTrue($Object.getPrototypeOf(target) === Object.prototype);

        // Array static methods.
        assert.AssertTrue($Array.isArray([1, 2]));
        assert.AssertTrue(!$Array.isArray('not array'));
        var fromArr = $Array.from('hi');
        assert.AssertTrue(
            fromArr.length === 2 &&
            fromArr[0] === 'h' &&
            fromArr[1] === 'i');

        // String static methods.
        assert.AssertTrue($String.fromCharCode(65, 66) === 'AB');

        // JSON static methods.
        var parsed = $JSON.parse('{"x":100}');
        assert.AssertTrue(parsed.x === 100);
      )");
  env()->module_system()->Require("test");
}

TEST_F(SafeBuiltinsUnittest, TestMicrotaskSuppression) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("test", R"(
        var assert = requireNative('assert');
        var microtaskExecuted = false;
        Promise.resolve().then(function() {
          microtaskExecuted = true;
        });
        assert.AssertTrue(!microtaskExecuted);
        $Array.forEach([1, 2], function(x) {
          assert.AssertTrue(!microtaskExecuted);
        });
        assert.AssertTrue(!microtaskExecuted);
      )");
  env()->module_system()->Require("test");
}

TEST_F(SafeBuiltinsUnittest, TestJSONStringifyToJSONAntiClobbering) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("test", R"(
        var assert = requireNative('assert');
        var customToJSONCalled = false;
        Array.prototype.toJSON = function() {
          customToJSONCalled = true;
          return 'clobbered array';
        };
        Date.prototype.toJSON = function() {
          customToJSONCalled = true;
          return 'clobbered date';
        };
        Object.prototype.toJSON = function() {
          customToJSONCalled = true;
          return 'clobbered object';
        };
        var obj = { arr: [1, 2], d: new Date(0) };
        var str = $JSON.stringify(obj);
        assert.AssertTrue(!customToJSONCalled);
        assert.AssertTrue(str !== undefined);
        assert.AssertTrue(str.indexOf('clobbered') === -1);
        assert.AssertTrue(typeof Array.prototype.toJSON === 'function');
        assert.AssertTrue(typeof Date.prototype.toJSON === 'function');
        assert.AssertTrue(typeof Object.prototype.toJSON === 'function');
      )");
  env()->module_system()->Require("test");
}

TEST_F(SafeBuiltinsUnittest, TestJSONStringifyAccessorToJSONAntiClobbering) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("test", R"(
        var assert = requireNative('assert');
        var customToJSONCalled = false;
        Object.defineProperty(Array.prototype, 'toJSON', {
          configurable: true,
          get: function() {
            customToJSONCalled = true;
            return function() { return 'clobbered accessor'; };
          },
          set: function(v) {}
        });
        var obj = { arr: [1, 2] };
        var str = $JSON.stringify(obj);
        assert.AssertTrue(!customToJSONCalled);
        assert.AssertTrue(str !== undefined);
        assert.AssertTrue(str.indexOf('clobbered') === -1);
      )");
  env()->module_system()->Require("test");
}

TEST_F(SafeBuiltinsUnittest,
       TestIndexedPropertyClobberingBeforeSafeBuiltinAccess) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());

  v8::Local<v8::Context> v8_context = env()->context()->v8_context();
  v8::Context::Scope context_scope(v8_context);
  v8::Isolate* isolate = env()->isolate();
  const char* clobber_script = R"(
    Object.defineProperty(Object.prototype, '0', {
      get: function() { throw new Error('clobbered 0'); }
    });
    Object.defineProperty(Array.prototype, '0', {
      get: function() { throw new Error('clobbered 0'); }
    });
  )";
  v8::Script::Compile(
      v8_context,
      v8::String::NewFromUtf8(isolate, clobber_script).ToLocalChecked())
      .ToLocalChecked()
      ->Run(v8_context)
      .ToLocalChecked();

  env()->RegisterModule("test", R"(
        var assert = requireNative('assert');
        assert.AssertTrue($Array.isArray([1, 2]));
        var parsed = $JSON.parse('{"x":100}');
        assert.AssertTrue(parsed.x === 100);
        assert.AssertTrue($JSON.stringify({a: 1}) === '{"a":1}');
      )");
  env()->module_system()->Require("test");
}

TEST_F(SafeBuiltinsUnittest, TestJSONStringifySwallowsExceptions) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());
  env()->RegisterModule("test", R"(
        var assert = requireNative('assert');

        // 1. Circular structure (should throw TypeError, but swallowed)
        var circular = {};
        circular.self = circular;
        var result1;
        var threw1 = false;
        try {
          result1 = $JSON.stringify(circular);
        } catch (e) {
          threw1 = true;
        }
        assert.AssertTrue(!threw1);
        assert.AssertTrue(result1 === undefined);

        // 2. User-defined toJSON throws (should throw, but swallowed)
        var obj = {
          toJSON: function() {
            throw new Error('toJSON error');
          }
        };
        var result2;
        var threw2 = false;
        try {
          result2 = $JSON.stringify(obj);
        } catch (e) {
          threw2 = true;
        }
        assert.AssertTrue(!threw2);
        assert.AssertTrue(result2 === undefined);
      )");
  env()->module_system()->Require("test");
}

// Tests that scripts that set custom setters before safe builtins are accessed
// don't intercept calls to certain properties. Regression test for
// https://crbug.com/538798091.
TEST_F(SafeBuiltinsUnittest,
       TestInheritedSetterPropertyClobberingBeforeSafeBuiltinAccess) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());

  v8::Local<v8::Context> v8_context = env()->context()->v8_context();
  v8::Context::Scope context_scope(v8_context);
  v8::Isolate* isolate = env()->isolate();
  const char* clobber_script = R"(
    Object.defineProperty(Function.prototype, 'self', {
      set: function(val) { throw new Error('clobbered self setter'); },
      configurable: true,
    });
    Object.defineProperty(Function.prototype, 'push', {
      set: function(val) { throw new Error('clobbered push setter'); },
      configurable: true,
    });
    Object.defineProperty(Object.prototype, 'parse', {
      set: function(val) { throw new Error('clobbered parse setter'); },
      configurable: true,
    });
    Object.defineProperty(Object.prototype, 'toStringTag', {
      set: function(val) { throw new Error('clobbered toStringTag setter'); },
      configurable: true,
    });
  )";
  v8::Script::Compile(
      v8_context,
      v8::String::NewFromUtf8(isolate, clobber_script).ToLocalChecked())
      .ToLocalChecked()
      ->Run(v8_context)
      .ToLocalChecked();

  env()->RegisterModule("test", R"(
        var assert = requireNative('assert');
        assert.AssertTrue(typeof $Array.push === 'function');
        assert.AssertTrue($Function.self === Function);
        assert.AssertTrue(typeof $JSON.parse === 'function');
        assert.AssertTrue($Symbol.toStringTag === Symbol.toStringTag);
      )");
  env()->module_system()->Require("test");
}

// Tests that other calls to Get() can't be intercepted.
TEST_F(SafeBuiltinsUnittest, TestJSONStringifyPrototypeGetterClobbering) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      env()->module_system());

  v8::Local<v8::Context> v8_context = env()->context()->v8_context();
  v8::Context::Scope context_scope(v8_context);
  v8::Isolate* isolate = env()->isolate();
  const char* clobber_script = R"(
    var getterCalled = false;
    Object.defineProperty(Object.prototype, 'configurable', {
      get: function() {
        getterCalled = true;
        return true;
      },
      configurable: true
    });
    Object.defineProperty(Object.prototype, 'writable', {
      get: function() {
        getterCalled = true;
        return true;
      },
      configurable: true
    });
    Array.prototype.toJSON = function() { return 'override'; };
  )";
  v8::Script::Compile(
      v8_context,
      v8::String::NewFromUtf8(isolate, clobber_script).ToLocalChecked())
      .ToLocalChecked()
      ->Run(v8_context)
      .ToLocalChecked();

  env()->RegisterModule("test", R"(
        var assert = requireNative('assert');
        var str = $JSON.stringify([1, 2]);
        assert.AssertTrue(str === '[1,2]');
        assert.AssertTrue(!getterCalled);
      )");
  env()->module_system()->Require("test");
}

}  // namespace
}  // namespace extensions
