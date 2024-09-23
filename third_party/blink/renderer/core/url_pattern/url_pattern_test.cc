#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpattern_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

v8::Local<v8::Value> Eval(V8TestingScope& scope, const char* source) {
  v8::Local<v8::Script> script =
      v8::Script::Compile(scope.GetContext(),
                          V8String(scope.GetIsolate(), source))
          .ToLocalChecked();
  return script->Run(scope.GetContext()).ToLocalChecked();
}

}  // namespace

TEST(URLPatternTest, CompatibleFromString) {
  test::TaskEnvironment task_environment;
  KURL base_url("https://urlpattern.example/foo/bar");
  V8TestingScope scope(base_url);
  v8::Local<v8::String> pattern_string =
      V8String(scope.GetIsolate(), "baz/:quux");
  auto* compatible = V8URLPatternCompatible::Create(
      scope.GetIsolate(), pattern_string, ASSERT_NO_EXCEPTION);
  auto* url_pattern = URLPattern::From(scope.GetIsolate(), compatible, base_url,
                                       ASSERT_NO_EXCEPTION);
  EXPECT_EQ(url_pattern->protocol(), "https");
  EXPECT_EQ(url_pattern->hostname(), "urlpattern.example");
  EXPECT_EQ(url_pattern->pathname(), "/foo/baz/:quux");
}

TEST(URLPatternTest, CompatibleFromStringInvalid) {
  test::TaskEnvironment task_environment;
  KURL base_url("https://urlpattern.example/foo/bar");
  V8TestingScope scope(base_url);
  v8::Local<v8::String> pattern_string = V8String(scope.GetIsolate(), "{");
  auto* compatible = V8URLPatternCompatible::Create(
      scope.GetIsolate(), pattern_string, ASSERT_NO_EXCEPTION);
  DummyExceptionStateForTesting exception_state;
  EXPECT_FALSE(URLPattern::From(scope.GetIsolate(), compatible, base_url,
                                exception_state));
  EXPECT_TRUE(exception_state.HadException());
}

TEST(URLPatternTest, CompatibleFromInit) {
  test::TaskEnvironment task_environment;
  KURL base_url("https://urlpattern.example/foo/bar");
  V8TestingScope scope(base_url);
  v8::Local<v8::Value> init = Eval(scope, "({search: 'a=42'})");
  ASSERT_TRUE(init->IsObject());
  auto* compatible = V8URLPatternCompatible::Create(scope.GetIsolate(), init,
                                                    ASSERT_NO_EXCEPTION);
  auto* url_pattern = URLPattern::From(scope.GetIsolate(), compatible, base_url,
                                       ASSERT_NO_EXCEPTION);
  EXPECT_EQ(url_pattern->protocol(), "https");
  EXPECT_EQ(url_pattern->hostname(), "urlpattern.example");
  EXPECT_EQ(url_pattern->pathname(), "/foo/bar");
  EXPECT_EQ(url_pattern->search(), "a=42");
}

TEST(URLPatternTest, CompatibleFromInitWithBaseURL) {
  test::TaskEnvironment task_environment;
  KURL base_url("https://urlpattern.example/foo/bar");
  V8TestingScope scope(base_url);
  v8::Local<v8::Value> init =
      Eval(scope, "({search: 'a=42', baseURL: 'https://alt.example/'})");
  ASSERT_TRUE(init->IsObject());
  auto* compatible = V8URLPatternCompatible::Create(scope.GetIsolate(), init,
                                                    ASSERT_NO_EXCEPTION);
  auto* url_pattern = URLPattern::From(scope.GetIsolate(), compatible, base_url,
                                       ASSERT_NO_EXCEPTION);
  EXPECT_EQ(url_pattern->protocol(), "https");
  EXPECT_EQ(url_pattern->hostname(), "alt.example");
  EXPECT_EQ(url_pattern->pathname(), "/");
  EXPECT_EQ(url_pattern->search(), "a=42");
}

TEST(URLPatternTest, CompatibleFromInitInvalid) {
  test::TaskEnvironment task_environment;
  KURL base_url("https://urlpattern.example/foo/bar");
  V8TestingScope scope(base_url);
  v8::Local<v8::Value> init = Eval(scope, "({hash: '{'})");
  ASSERT_TRUE(init->IsObject());
  auto* compatible = V8URLPatternCompatible::Create(scope.GetIsolate(), init,
                                                    ASSERT_NO_EXCEPTION);
  DummyExceptionStateForTesting exception_state;
  EXPECT_FALSE(URLPattern::From(scope.GetIsolate(), compatible, base_url,
                                exception_state));
  EXPECT_TRUE(exception_state.HadException());
}

TEST(URLPatternTest, CompatibleFromURLPattern) {
  test::TaskEnvironment task_environment;
  KURL base_url("https://urlpattern.example/foo/bar");
  V8TestingScope scope(base_url);
  v8::Local<v8::Value> wrapper =
      Eval(scope, "new URLPattern({protocol: 'https'})");
  ASSERT_TRUE(V8URLPattern::HasInstance(scope.GetIsolate(), wrapper));
  auto* compatible = V8URLPatternCompatible::Create(scope.GetIsolate(), wrapper,
                                                    ASSERT_NO_EXCEPTION);
  auto* url_pattern = URLPattern::From(scope.GetIsolate(), compatible, base_url,
                                       ASSERT_NO_EXCEPTION);
  EXPECT_EQ(url_pattern, V8URLPattern::ToWrappable(scope.GetIsolate(),
                                                   wrapper.As<v8::Object>()));
}

}  // namespace blink
