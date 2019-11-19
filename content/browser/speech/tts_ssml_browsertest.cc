// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/speech/tts_controller_impl.h"

#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"

// TODO(crbug.com/961029): Fix memory leaks in tests and re-enable on LSAN.
#ifdef LEAK_SANITIZER
#define MAYBE_TestStripSSML DISABLED_TestStripSSML
#else
#define MAYBE_TestStripSSML TestStripSSML
#endif

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
    MockTtsControllerImpl* controller = new MockTtsControllerImpl();

    std::unique_ptr<TtsUtterance> utterance = TtsUtterance::Create(nullptr);
    utterance->SetText(input);

    base::RunLoop run_loop;
    controller->StripSSML(
        utterance->GetText(),
        base::BindOnce(&TtsSsmlBrowserTest::CheckCorrect,
                       base::Unretained(this), run_loop.QuitClosure(),
                       expected_string));
    run_loop.Run();

    delete controller;
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

IN_PROC_BROWSER_TEST_F(TtsSsmlBrowserTest, MAYBE_TestStripSSML) {
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
