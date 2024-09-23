/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace test {

namespace {

base::FilePath BlinkRootFilePath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
  return base::MakeAbsoluteFilePath(
      path.Append(FILE_PATH_LITERAL("third_party/blink")));
}

base::FilePath WebTestsFilePath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
  return base::MakeAbsoluteFilePath(
      path.Append(FILE_PATH_LITERAL("third_party/blink/web_tests")));
}

}  // namespace

void RunPendingTasks() {
  base::RunLoop loop;
  scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
      FROM_HERE, WTF::BindOnce(loop.QuitWhenIdleClosure()));
  loop.Run();
}

void RunDelayedTasks(base::TimeDelta delay) {
  base::RunLoop loop;
  scheduler::GetSingleThreadTaskRunnerForTesting()->PostDelayedTask(
      FROM_HERE, WTF::BindOnce(loop.QuitWhenIdleClosure()), delay);
  loop.Run();
}

void YieldCurrentThread() {
  base::PlatformThread::YieldCurrentThread();
}

String BlinkRootDir() {
  return FilePathToWebString(BlinkRootFilePath());
}

String BlinkWebTestsDir() {
  return FilePathToWebString(WebTestsFilePath());
}

String ExecutableDir() {
  base::FilePath path;
  base::PathService::Get(base::DIR_EXE, &path);
  return FilePathToWebString(base::MakeAbsoluteFilePath(path));
}

String CoreTestDataPath(const String& relative_path) {
  return FilePathToWebString(
      BlinkRootFilePath()
          .Append(FILE_PATH_LITERAL("renderer/core/testing/data"))
          .Append(WebStringToFilePath(relative_path)));
}

String PlatformTestDataPath(const String& relative_path) {
  return FilePathToWebString(
      BlinkRootFilePath()
          .Append(FILE_PATH_LITERAL("renderer/platform/testing/data"))
          .Append(WebStringToFilePath(relative_path)));
}

String AccessibilityTestDataPath(const String& relative_path) {
  return FilePathToWebString(
      BlinkRootFilePath()
          .Append(
              FILE_PATH_LITERAL("renderer/modules/accessibility/testing/data"))
          .Append(WebStringToFilePath(relative_path)));
}

base::FilePath HyphenationDictionaryDir() {
  base::FilePath exe_dir;
  base::PathService::Get(base::DIR_EXE, &exe_dir);
  return exe_dir.AppendASCII("gen/hyphen-data");
}

std::optional<Vector<char>> ReadFromFile(const String& path) {
  base::FilePath file_path = blink::WebStringToFilePath(path);
  std::string buffer;
  if (!base::ReadFileToString(file_path, &buffer)) {
    return std::nullopt;
  }
  return Vector<char>(buffer);
}

String BlinkWebTestsFontsTestDataPath(const String& relative_path) {
  return FilePathToWebString(
      WebTestsFilePath()
          .Append(FILE_PATH_LITERAL("external/wpt/fonts"))
          .Append(WebStringToFilePath(relative_path)));
}

String BlinkWebTestsImagesTestDataPath(const String& relative_path) {
  return FilePathToWebString(WebTestsFilePath()
                                 .Append(FILE_PATH_LITERAL("images/resources"))
                                 .Append(WebStringToFilePath(relative_path)));
}

String StylePerfTestDataPath(const String& relative_path) {
  return FilePathToWebString(
      BlinkRootFilePath()
          .Append(FILE_PATH_LITERAL("renderer/core/css/perftest_data"))
          .Append(WebStringToFilePath(relative_path)));
}

LineReader::LineReader(const String& text) : text_(text), index_(0) {}

bool LineReader::GetNextLine(String* line) {
  if (index_ >= text_.length())
    return false;

  wtf_size_t end_of_line_index = text_.Find("\r\n", index_);
  if (end_of_line_index == kNotFound) {
    *line = text_.Substring(index_);
    index_ = text_.length();
    return true;
  }

  *line = text_.Substring(index_, end_of_line_index - index_);
  index_ = end_of_line_index + 2;
  return true;
}

}  // namespace test
}  // namespace blink
