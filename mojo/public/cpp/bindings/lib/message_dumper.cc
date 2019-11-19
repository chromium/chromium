// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/message_dumper.h"

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "mojo/public/cpp/bindings/message.h"

namespace {

base::FilePath& DumpDirectory() {
  static base::NoDestructor<base::FilePath> dump_directory;
  return *dump_directory;
}

void WriteMessage(uint64_t identifier,
                  const mojo::MessageDumper::MessageEntry& entry) {
  static uint64_t num = 0;

  if (!entry.interface_name)
    return;

  base::FilePath message_directory =
      DumpDirectory()
          .AppendASCII(entry.interface_name)
          .AppendASCII(base::NumberToString(identifier));

  if (!base::DirectoryExists(message_directory) &&
      !base::CreateDirectory(message_directory)) {
    LOG(ERROR) << "Failed to create" << message_directory.value();
    return;
  }

  std::string filename =
      base::NumberToString(num++) + "." + entry.method_name + ".mojomsg";
  base::FilePath path = message_directory.AppendASCII(filename);
  base::File file(path,
                  base::File::FLAG_WRITE | base::File::FLAG_CREATE_ALWAYS);

  file.WriteAtCurrentPos(reinterpret_cast<const char*>(entry.data_bytes.data()),
                         static_cast<int>(entry.data_bytes.size()));
}

}  // namespace

namespace mojo {

MessageDumper::MessageEntry::MessageEntry(const uint8_t* data,
                                          uint32_t data_size,
                                          const char* interface_name,
                                          const char* method_name)
    : interface_name(interface_name),
      method_name(method_name),
      data_bytes(data, data + data_size) {}

MessageDumper::MessageEntry::MessageEntry(const MessageEntry& entry) = default;

MessageDumper::MessageEntry::~MessageEntry() {}

MessageDumper::MessageDumper() : identifier_(base::RandUint64()) {}

MessageDumper::~MessageDumper() {}

bool MessageDumper::Accept(mojo::Message* message) {
  MessageEntry entry(message->data(), message->data_num_bytes(),
                     message->interface_name(), message->method_name());

  static base::NoDestructor<scoped_refptr<base::TaskRunner>> task_runner(
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));

  (*task_runner)
      ->PostTask(FROM_HERE,
                 base::BindOnce(&WriteMessage, identifier_, std::move(entry)));
  return true;
}

void MessageDumper::SetMessageDumpDirectory(const base::FilePath& directory) {
  DumpDirectory() = directory;
}

const base::FilePath& MessageDumper::GetMessageDumpDirectory() {
  return DumpDirectory();
}

}  // namespace mojo
