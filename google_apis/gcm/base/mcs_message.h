// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_BASE_MCS_MESSAGE_H_
#define GOOGLE_APIS_GCM_BASE_MCS_MESSAGE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "google_apis/gcm/base/gcm_export.h"

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace gcm {

// A wrapper for MCS protobuffers that encapsulates their tag, size and data
// in an immutable and thread-safe format. If a mutable version is desired,
// CloneProtobuf() should use used to create a new copy of the protobuf.
//
// Note: default copy and assign welcome.
class GCM_EXPORT MCSMessage {
 public:
  // Creates an invalid MCSMessage.
  MCSMessage();
  // Infers tag from |message|.
  explicit MCSMessage(const google::protobuf::MessageLite& protobuf);
  // |tag| must match |protobuf|'s message type.
  MCSMessage(uint8_t tag, const google::protobuf::MessageLite& protobuf);
  // |tag| must match |protobuf|'s message type. Takes ownership of |protobuf|.
  MCSMessage(uint8_t tag,
             std::unique_ptr<const google::protobuf::MessageLite> protobuf);
  MCSMessage(const MCSMessage& other);
  ~MCSMessage();

  // Returns whether this message is valid or not (whether a protobuf was
  // provided at construction time or not).
  bool IsValid() const;

  // Getters for serialization.
  uint8_t tag() const { return tag_; }
  int size() const {return size_; }
  std::string SerializeAsString() const;

  // Getter for accessing immutable probotuf fields.
  const google::protobuf::MessageLite& GetProtobuf() const;

  // Getter for creating a mutated version of the protobuf.
  std::unique_ptr<google::protobuf::MessageLite> CloneProtobuf() const;

 private:
  class Core : public base::RefCountedThreadSafe<MCSMessage::Core> {
   public:
    Core();
    Core(uint8_t tag, const google::protobuf::MessageLite& protobuf);
    Core(uint8_t tag,
         std::unique_ptr<const google::protobuf::MessageLite> protobuf);

    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;

    const google::protobuf::MessageLite& Get() const;

   private:
    friend class base::RefCountedThreadSafe<MCSMessage::Core>;
    ~Core();

    // The immutable protobuf.
    std::unique_ptr<const google::protobuf::MessageLite> protobuf_;
  };

  // These are cached separately to avoid having to recompute them.
  const uint8_t tag_;
  const int size_;

  // The refcounted core, containing the protobuf memory.
  scoped_refptr<const Core> core_;
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_BASE_MCS_MESSAGE_H_
