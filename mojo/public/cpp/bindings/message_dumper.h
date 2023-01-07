// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_DUMPER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_DUMPER_H_

#include "base/files/file_path.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {

class MessageDumper : public mojo::MessageReceiver {
 public:
  MessageDumper();
  ~MessageDumper() override;

  bool Accept(mojo::Message* message) override;

  struct MessageEntry {
    MessageEntry(const uint8_t* data,
                 uint32_t data_size,
                 const char* interface_name,
                 const char* method_name);
    MessageEntry(const MessageEntry& entry);
    ~MessageEntry();

    const char* interface_name;
    const char* method_name;
    std::vector<uint8_t> data_bytes;
  };

  static void SetMessageDumpDirectory(const base::FilePath& directory);
  static const base::FilePath& GetMessageDumpDirectory();

 private:
  uint64_t identifier_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_DUMPER_H_
