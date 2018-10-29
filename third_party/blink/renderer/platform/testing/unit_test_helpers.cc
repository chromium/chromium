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

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {
namespace test {

namespace {

base::FilePath BlinkRootFilePath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
  return base::MakeAbsoluteFilePath(
      path.Append(FILE_PATH_LITERAL("third_party/blink")));
}

// TODO(tkent): Rename this function.  crbug.com/843412.
base::FilePath LayoutTestsFilePath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
  base::FilePath web_tests_path = base::MakeAbsoluteFilePath(
      path.Append(FILE_PATH_LITERAL("third_party/blink/web_tests")));
  if (base::PathExists(web_tests_path))
    return web_tests_path;
  return base::MakeAbsoluteFilePath(
      path.Append(FILE_PATH_LITERAL("third_party/WebKit/LayoutTests")));
}

}  // namespace

void RunPendingTasks() {
  Platform::Current()->CurrentThread()->GetTaskRunner()->PostTask(
      FROM_HERE, WTF::Bind(&ExitRunLoop));

  // We forbid GC in the tasks. Otherwise the registered GCTaskObserver tries
  // to run GC with NoHeapPointerOnStack.
  ThreadState::Current()->EnterGCForbiddenScope();
  EnterRunLoop();
  ThreadState::Current()->LeaveGCForbiddenScope();
}

void RunDelayedTasks(TimeDelta delay) {
  Platform::Current()->CurrentThread()->GetTaskRunner()->PostDelayedTask(
      FROM_HERE, WTF::Bind(&ExitRunLoop), delay);
  EnterRunLoop();
}

void EnterRunLoop() {
  base::RunLoop().Run();
}

void ExitRunLoop() {
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

void YieldCurrentThread() {
  base::PlatformThread::YieldCurrentThread();
}

String BlinkRootDir() {
  return FilePathToWebString(BlinkRootFilePath());
}

String BlinkLayoutTestsDir() {
  return FilePathToWebString(LayoutTestsFilePath());
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

scoped_refptr<SharedBuffer> ReadFromFile(const String& path) {
  base::FilePath file_path = blink::WebStringToFilePath(path);
  std::string buffer;
  base::ReadFileToString(file_path, &buffer);
  return SharedBuffer::Create(buffer.data(), buffer.size());
}

LineReader::LineReader(const std::string& text) : text_(text), index_(0) {}

bool LineReader::GetNextLine(std::string* line) {
  line->clear();
  if (index_ >= text_.length())
    return false;

  size_t end_of_line_index = text_.find("\r\n", index_);
  if (end_of_line_index == std::string::npos) {
    *line = text_.substr(index_);
    index_ = text_.length();
    return true;
  }

  *line = text_.substr(index_, end_of_line_index - index_);
  index_ = end_of_line_index + 2;
  return true;
}

}  // namespace test
}  // namespace blink
