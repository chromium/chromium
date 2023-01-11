// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/tts_controller_impl.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"

namespace content {

namespace {

// Tts Controller implementation that does nothing.
class MockTtsControllerImpl : public TtsControllerImpl {
 public:
  MockTtsControllerImpl() {}
  ~MockTtsControllerImpl() override {}
};

class TtsSsmlBrowserTest : public ContentBrowserTest {
 public:
  // If no SSML is stripped, then we want input == output.
  void RunNoStripSSMLTest(std::string input) { RunSSMLStripTest(input, input); }

  void RunSSMLStripTest(std::string input, std::string expected_string) {
    std::unique_ptr<MockTtsControllerImpl> controller =
        std::make_unique<MockTtsControllerImpl>();

    std::unique_ptr<TtsUtterance> utterance = TtsUtterance::Create();
    utterance->SetText(input);

    base::RunLoop run_loop;
    controller->StripSSML(
        utterance->GetText(),
        base::BindOnce(&TtsSsmlBrowserTest::CheckCorrect,
                       base::Unretained(this), run_loop.QuitClosure(),
                       expected_string));
    run_loop.Run();
  }

  // Passed as callback to StripSSML.
  void CheckCorrect(base::OnceClosure quit_loop_closure,
                    const std::string& expected_string,
                    const std::string& actual_string) {
    EXPECT_EQ(expected_string, actual_string);
    base::ScopedClosureRunner runner(std::move(quit_loop_closure));
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(TtsSsmlBrowserTest, TestStripSSML) {
  // No SSML should be stripped.
  RunNoStripSSMLTest("");
  RunNoStripSSMLTest("What if I told you that 5 < 4?");
  RunNoStripSSMLTest("What if I told you that  4 > 5?");
  RunNoStripSSMLTest("Truth is, 4 < 5! And 5 > 4!");
  RunNoStripSSMLTest(
      "<?xml version='1.0'?><paragraph>Hello world<speak>Invalid "
      "ssml</speak></paragraph>");
  RunNoStripSSMLTest(
      "<?xml version='1.0'?><paragraph><sentence>Invalid"
      "SSML</sentence></paragraph>");

  // SSML should be stripped.
  RunSSMLStripTest("<?xml version='1.0'?><speak>Hello world</speak>",
                   "Hello world");
  RunSSMLStripTest(
      "<?xml version='1.0'?>"
      "<speak>"
      "<voice gender='female'>Any female voice here."
      "<voice category='child'>"
      "A female child voice here."
      "<paragraph xml:lang='ja'>"
      "こんにちは"
      "</paragraph>"
      "</voice>"
      "</voice>"
      "</speak>",
      "Any female voice here.A female child voice here.こんにちは");
  RunSSMLStripTest(
      "<?xml version='1.0'?>"
      "<speak>The <emphasis>second</emphasis> word of this sentence was "
      "emphasized.</speak>",
      "The second word of this sentence was emphasized.");
  RunSSMLStripTest(
      "<?xml version='1.0'?>"
      "<!-- Ignore this -->"
      "<speak xml:lang='en-US'>"
      "<paragraph>I would like to have a hamburger.</paragraph>"
      "</speak>",
      "I would like to have a hamburger.");
}

}  // namespace content
