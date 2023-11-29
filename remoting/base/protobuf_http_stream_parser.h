// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PROTOBUF_HTTP_STREAM_PARSER_H_
#define REMOTING_BASE_PROTOBUF_HTTP_STREAM_PARSER_H_

#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/protobuf/src/google/protobuf/wire_format_lite.h"

namespace google {
namespace protobuf {
namespace io {
class CodedInputStream;
}  // namespace io
}  // namespace protobuf
}  // namespace google

namespace net {
class GrowableIOBuffer;
}  // namespace net

namespace remoting {

class ProtobufHttpStatus;

// Class to parse incoming stream data wrapped with a StreamBody protobuf
// message.
class ProtobufHttpStreamParser final {
 public:
  using MessageCallback = base::RepeatingCallback<void(const std::string&)>;
  using StreamClosedCallback =
      base::OnceCallback<void(const ProtobufHttpStatus&)>;

  ProtobufHttpStreamParser(const MessageCallback& message_callback,
                           StreamClosedCallback stream_closed_callback);
  ~ProtobufHttpStreamParser();

  ProtobufHttpStreamParser(const ProtobufHttpStreamParser&) = delete;
  ProtobufHttpStreamParser& operator=(const ProtobufHttpStreamParser&) = delete;

  // Appends the stream data (which should be the partial or full serialized
  // StreamBody) and runs callbacks if there is something decodable.
  void Append(std::string_view data);

  // Indicates whether the parser has pending data that needs more input to
  // complete a StreamBody message.
  bool HasPendingData() const;

 private:
  void ParseStreamIfAvailable();
  bool ParseOneField(google::protobuf::io::CodedInputStream* input_stream);

  // This also closes the stream if the wire type is invalid.
  bool ValidateWireType(
      int field_number,
      google::protobuf::internal::WireFormatLite::WireType wire_type);

  MessageCallback message_callback_;
  StreamClosedCallback stream_closed_callback_;
  scoped_refptr<net::GrowableIOBuffer> read_buffer_;

  base::WeakPtrFactory<ProtobufHttpStreamParser> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_BASE_PROTOBUF_HTTP_STREAM_PARSER_H_
