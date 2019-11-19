// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_MULTIPART_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_MULTIPART_PARSER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// This class parses a multipart message which is supplied (possible in chunks)
// to MultipartParser::appendData which parses the data and passes resulting
// part header fields and data to Client.
//
// - MultipartParser::appendData does not accept base64, quoted-printable nor
//   otherwise transfer encoded multipart message parts (no-op transfer
//   encodings "binary", "7bit" and "8bit" are OK).
// - If MultipartParser::cancel() is called, Client's methods will not be
//   called anymore.
class CORE_EXPORT MultipartParser final
    : public GarbageCollected<MultipartParser> {
 public:
  // Client recieves parsed part header fields and data.
  class CORE_EXPORT Client : public GarbageCollectedMixin {
   public:
    virtual ~Client() = default;
    // The method is called whenever header fields of a part are parsed.
    virtual void PartHeaderFieldsInMultipartReceived(
        const HTTPHeaderMap& header_fields) = 0;
    // The method is called whenever some data of a part is parsed which
    // can happen zero or more times per each part. It always holds that
    // |size| > 0.
    virtual void PartDataInMultipartReceived(const char* bytes, size_t) = 0;
    // The method is called whenever all data of a complete part is parsed.
    virtual void PartDataInMultipartFullyReceived() = 0;
    void Trace(blink::Visitor* visitor) override {}
  };

  MultipartParser(Vector<char> boundary, Client*);
  bool AppendData(const char* bytes, size_t);
  void Cancel();
  bool Finish();

  bool IsCancelled() const { return state_ == State::kCancelled; }

  void Trace(blink::Visitor*);

 private:
  class Matcher {
    DISALLOW_NEW();

   public:
    Matcher();
    Matcher(const char* data, size_t num_matched_bytes, size_t);

    bool Match(char value) {
      DCHECK_LT(num_matched_bytes_, size_);
      if (value != data_[num_matched_bytes_])
        return false;
      ++num_matched_bytes_;
      return true;
    }
    bool Match(const char* first, const char* last);
    bool IsMatchComplete() const { return num_matched_bytes_ == size_; }
    size_t NumMatchedBytes() const { return num_matched_bytes_; }
    void SetNumMatchedBytes(size_t);

    const char* Data() const { return data_; }

   private:
    const char* data_ = nullptr;
    size_t num_matched_bytes_ = 0u;
    size_t size_ = 0u;
  };

  Matcher CloseDelimiterSuffixMatcher() const;
  Matcher DelimiterMatcher(size_t num_already_matched_bytes = 0u) const;
  Matcher DelimiterSuffixMatcher() const;

  void ParseDataAndDelimiter(const char** bytes_pointer, const char* bytes_end);
  void ParseDelimiter(const char** bytes_pointer, const char* bytes_end);
  bool ParseHeaderFields(const char** bytes_pointer,
                         const char* bytes_end,
                         HTTPHeaderMap* header_fields);
  void ParseTransportPadding(const char** bytes_pointer,
                             const char* bytes_end) const;

  Matcher matcher_;
  Vector<char> buffered_header_bytes_;
  Member<Client> client_;
  Vector<char> delimiter_;

  enum class State {
    kParsingPreamble,
    kParsingDelimiterSuffix,
    kParsingPartHeaderFields,
    kParsingPartOctets,
    kParsingDelimiterOrCloseDelimiterSuffix,
    kParsingCloseDelimiterSuffix,
    kParsingEpilogue,
    kCancelled,
    kFinished
  } state_;

  DISALLOW_COPY_AND_ASSIGN(MultipartParser);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_MULTIPART_PARSER_H_
