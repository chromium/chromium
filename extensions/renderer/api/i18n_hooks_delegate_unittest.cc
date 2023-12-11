// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/i18n_hooks_delegate.h"

#include "base/strings/stringprintf.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/shared_l10n_map.h"

namespace extensions {

using I18nHooksDelegateTest = NativeExtensionBindingsSystemUnittest;

TEST_F(I18nHooksDelegateTest, TestI18nGetMessage) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("foo").Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = MainContext();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), mojom::ContextType::kPrivilegedExtension);
  script_context->set_url(extension->url());
  bindings_system()->UpdateBindingsForContext(script_context);

  // In practice, messages will be retrieved from the browser process on first
  // request. Since this is a unittest, pre-populate the message bundle.
  {
    SharedL10nMap::L10nMessagesMap messages = {
        {"simple", "simple message"},
        {"one_placeholder", "placeholder $1 end"},
        {"multi_placeholders", "placeholder $1 and $2 end"},
        {"special_characters", "< Hello $1 World &gt;"}};
    SharedL10nMap::GetInstance().SetMessagesForTesting(extension->id(),
                                                       std::move(messages));
  }

  auto run_get_message = [context](const char* args) {
    SCOPED_TRACE(args);
    constexpr char kRunGetMessageFunction[] =
        "(function() { return chrome.i18n.getMessage(%s); })";
    v8::Local<v8::Function> function = FunctionFromString(
        context, base::StringPrintf(kRunGetMessageFunction, args));
    v8::Local<v8::Value> result = RunFunction(function, context, 0, nullptr);
    return V8ToString(result, context);
  };

  // Simple tests.

  EXPECT_EQ(R"("simple message")", run_get_message("'simple'"));
  EXPECT_EQ(R"("placeholder foo end")",
            run_get_message("'one_placeholder', 'foo'"));
  EXPECT_EQ(R"("placeholder foo end")",
            run_get_message("'one_placeholder', ['foo']"));
  EXPECT_EQ(R"("placeholder foo and bar end")",
            run_get_message("'multi_placeholders', ['foo', 'bar']"));
  EXPECT_EQ(R"("\u003C Hello \u003Cbr> World &gt;")",
            run_get_message("'special_characters', ['<br>'], {}"));
  EXPECT_EQ(
      R"("\u003C Hello \u003Cbr> World &gt;")",
      run_get_message("'special_characters', ['<br>'], {escapeLt: false}"));
  EXPECT_EQ(
      R"("&lt; Hello \u003Cbr> World &gt;")",
      run_get_message("'special_characters', ['<br>'], {escapeLt: true}"));

  // We place the somewhat-arbitrary (but documented) limit of 9 substitutions
  // on the call.
  EXPECT_EQ("undefined",
            run_get_message("'one_placeholder',"
                            "['one', 'two', 'three', 'four', 'five', 'six',"
                            " 'seven', 'eight', 'nine', 'ten']"));

  // Oddities. All of these should probably behave differently. These tests are
  // more for documentation than for desirable functionality.

  // Non-string values passed in the array of placeholders will be implicitly
  // converted to strings...
  EXPECT_EQ(R"("placeholder [object Object] end")",
            run_get_message("'one_placeholder', [{}]"));
  // ... While non-string values passed as a single placeholder are silently
  // ignored.
  EXPECT_EQ(R"("placeholder  end")", run_get_message("'one_placeholder', {}"));
  // And values can throw errors (which are silently caught) in string
  // conversions, in which case the value is silently ignored.
  EXPECT_EQ(R"("placeholder  end")",
            run_get_message("'one_placeholder',"
                            "[{toString() { throw new Error('haha'); } }]"));
  EXPECT_EQ("undefined",
            run_get_message("'one_placeholder',"
                            "(function() {"
                            "    var x = [];"
                            "    Object.defineProperty(x, 0, {"
                            "        get() { throw new Error('haha'); }"
                            "    });"
                            "    return x;"
                            " })()"));
  EXPECT_EQ(
      R"("placeholder foo end")",
      run_get_message("'one_placeholder',"
                      "[{toString() { throw new Error('haha'); } }, 'foo']"));
}

}  // namespace extensions
