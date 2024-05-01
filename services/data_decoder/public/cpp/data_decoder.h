// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_PUBLIC_CPP_DATA_DECODER_H_
#define SERVICES_DATA_DECODER_PUBLIC_CPP_DATA_DECODER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/structured_headers.h"
#include "services/data_decoder/public/cpp/service_provider.h"
#include "services/data_decoder/public/mojom/data_decoder_service.mojom.h"
#include "services/data_decoder/public/mojom/xml_parser.mojom.h"

namespace mojo_base {
class BigBuffer;
}

namespace data_decoder {

// Encapsulates an exclusive connection to an isolated instance of the Data
// Decoder service, allowing an owner to perform a series of related decoding
// operations using the same isolated instance. The application must provide
// an instance of |ServiceProvider| via |ServiceProvider::Set()| prior to using
// this class.
//
// In general, instance reuse should only be considered after weighing the cost
// of new service processes vs the security and privacy value of increased
// isolation.
//
// Note that on some platforms, some operations (like JSON parsing on Android)
// use a safe in-process mechanism in lieu of delegating to the Data Decoder
// service. This detail is intentionally hidden behind the DataDecoder API.
//
// Finally, there is no guarantee that a single DataDecoder instance will
// perform all out-of-process operations within the same service process; if
// idle for long periods of time, the service process may be killed and only
// restarted once needed again.
//
// Tests can construct an data_decoder::test::InProcessDataDecoderService to
// ensure that all DataDecoders constructed during its lifetime will connect to
// that instance rather than launching a separate process.
class DataDecoder {
 public:
  // Creates a DataDecoder with an implementation-defined default timeout.
  DataDecoder();
  // Creates a DataDecoder with the specified timeout.
  explicit DataDecoder(base::TimeDelta idle_timeout);

  DataDecoder(const DataDecoder&) = delete;
  DataDecoder& operator=(const DataDecoder&) = delete;

  ~DataDecoder();

  using ValueOrError = base::expected<base::Value, std::string>;
  template <typename T>
  using ResultCallback =
      base::OnceCallback<void(base::expected<T, std::string>)>;
  using StructuredHeaderParseItemCallback =
      ResultCallback<net::structured_headers::ParameterizedItem>;
  using StructuredHeaderParseListCallback =
      ResultCallback<net::structured_headers::List>;
  using StructuredHeaderParseDictionaryCallback =
      ResultCallback<net::structured_headers::Dictionary>;
  using ValueParseCallback = ResultCallback<base::Value>;
  using GzipperCallback = ResultCallback<mojo_base::BigBuffer>;
  using ValidationCallback = ResultCallback<bool>;
  using CancellationFlag = base::RefCountedData<bool>;

  // Returns a raw interface to the service instance. This launches an instance
  // of the service process if possible on the current platform, or returns a
  // connection to the in-process instance of in a test environment using
  // InProcessDataDecoderService.
  mojom::DataDecoderService* GetService();

  // Parses the potentially unsafe JSON string in |json| using this
  // DataDecoder's service instance or some other platform-specific decoding
  // facility. The parser conforms to RFC 8259.
  //
  // Note that |callback| will only be called if the parsing operation succeeds
  // or fails before this DataDecoder is destroyed.
  void ParseJson(const std::string& json, ValueParseCallback callback);

  // Parses the potentially unsafe JSON string in |json|. This static helper
  // uses a dedicated instance of the Data Decoder service on applicable
  // platforms.
  static void ParseJsonIsolated(const std::string& json,
                                ValueParseCallback callback);

  // Parses the potentially unsafe string in |header| as a structured header
  // item using this DataDecoder's service instance or some other
  // platform-specific decoding facility.
  //
  // Note that |callback| will only be called if the parsing operation succeeds
  // or fails before this DataDecoder is destroyed.
  void ParseStructuredHeaderItem(const std::string& header,
                                 StructuredHeaderParseItemCallback callback);

  // Parses the potentially unsafe string in |header| as a structured header
  // item. This static helper uses a dedicated instance of the Data Decoder
  // service on applicable platforms.
  static void ParseStructuredHeaderItemIsolated(
      const std::string& header,
      StructuredHeaderParseItemCallback callback);

  // Parses the potentially unsafe string in |header| as a structured header
  // list using this DataDecoder's service instance or some other
  // platform-specific decoding facility.
  //
  // Note that |callback| will only be called if the parsing operation succeeds
  // or fails before this DataDecoder is destroyed.
  void ParseStructuredHeaderList(const std::string& header,
                                 StructuredHeaderParseListCallback callback);

  // Parses the potentially unsafe string in |header| as a structured header
  // list. This static helper uses a dedicated instance of the Data Decoder
  // service on applicable platforms.
  static void ParseStructuredHeaderListIsolated(
      const std::string& header,
      StructuredHeaderParseListCallback callback);

  // Parses the potentially unsafe string in `header` as a structured header
  // dictionary using this DataDecoder's service instance or some other
  // platform-specific decoding facility.
  //
  // Note that `callback` will only be called if the parsing operation succeeds
  // or fails before this DataDecoder is destroyed.
  void ParseStructuredHeaderDictionary(
      const std::string& header,
      StructuredHeaderParseDictionaryCallback callback);

  // Parses the potentially unsafe string in `header` as a structured header
  // dictionary. This static helper uses a dedicated instance of the Data
  // Decoder service on applicable platforms.
  static void ParseStructuredHeaderDictionaryIsolated(
      const std::string& header,
      StructuredHeaderParseDictionaryCallback callback);

  // Parses the potentially unsafe XML string in |xml| using this
  // DataDecoder's service instance. The Value provided to the callback
  // is a structured tree representing the XML document. See
  // ../mojom/xml_parser.mojom for details on the structure, and
  // safe_xml_parser.h for utilities to access parsed data.
  //
  // Note that |callback| will only be called if the parsing operation succeeds
  // or fails before this DataDecoder is destroyed.
  void ParseXml(const std::string& xml,
                mojom::XmlParser::WhitespaceBehavior whitespace_behavior,
                ValueParseCallback callback);

  // Parses the potentially unsafe XML string in |xml|. This static helper
  // uses a dedicated instance of the Data Decoder service on applicable
  // platforms.
  static void ParseXmlIsolated(
      const std::string& xml,
      mojom::XmlParser::WhitespaceBehavior whitespace_behavior,
      ValueParseCallback callback);

  // Deflates potentially unsafe |data| using this DataDecoder's service
  // instance. This will use raw DEFLATE, i.e. no headers are outputted.
  //
  // Note that |callback| will only be called if the parsing operation succeeds
  // or fails before this DataDecoder is destroyed.
  void Deflate(base::span<const uint8_t> data, GzipperCallback callback);

  // Inflates potentially unsafe |data| using this DataDecoder's service
  // instance. |data| must have been deflated raw, i.e. with no headers. If the
  // uncompressed data exceeds |max_uncompressed_size|, returns empty.
  //
  // Note that |callback| will only be called if the parsing operation succeeds
  // or fails before this DataDecoder is destroyed.
  void Inflate(base::span<const uint8_t> data,
               uint64_t max_uncompressed_size,
               GzipperCallback callback);

  // Compresses potentially unsafe |data| using this DataDecoder's service
  // instance.
  //
  // Note that |callback| will only be called if the parsing operation succeeds
  // or fails before this DataDecoder is destroyed.
  void GzipCompress(base::span<const uint8_t> data, GzipperCallback callback);

  // Uncompresses potentially unsafe |data| using this DataDecoder's service
  // instance.
  //
  // Note that |callback| will only be called if the parsing operation succeeds
  // or fails before this DataDecoder is destroyed.
  void GzipUncompress(base::span<const uint8_t> data, GzipperCallback callback);

  // Parses the potentially unsafe CBOR bytes in |cbor| using this
  // DataDecoder's service instance or some other platform-specific decoding
  // facility. The parser conforms to RFC 7049, except a few limitations:
  // - Does not support null or undefined values.
  // - Integers must fit in the 'int' type.
  // - The keys in Maps must be a string or byte-string.
  // - If at least one Map key is invalid, an error will be returned.
  //
  // Note that |callback| will only be called if the parsing operation succeeds
  // or fails before this DataDecoder is destroyed.
  void ParseCbor(base::span<const uint8_t> cbor, ValueParseCallback callback);

  // Parses the potentially unsafe CBOR bytes in |cbor|. This static helper
  // uses a dedicated instance of the Data Decoder service on applicable
  // platforms.
  static void ParseCborIsolated(base::span<const uint8_t> cbor,
                                ValueParseCallback callback);

  // Validates the format of the potentially unsafe `pix_code`.
  void ValidatePixCode(const std::string& pix_code,
                       ValidationCallback callback);

 private:
  // The amount of idle time to tolerate on a DataDecoder instance. If the
  // instance is unused for this period of time, the underlying service process
  // (if any) may be killed and only restarted once needed again.
  // On platforms (like iOS) or environments (like some unit tests) where
  // out-of-process services are not used, this has no effect.
  base::TimeDelta idle_timeout_;

  // This instance's connection to the service. This connection is lazily
  // established and may be reset after long periods of idle time.
  mojo::Remote<mojom::DataDecoderService> service_;

  // Cancellation flag for any outstanding requests. When a request is
  // started, it takes a reference to this flag. Upon the destruction of this
  // instance, the flag is set to `true`. Any outstanding requests should check
  // this flag, and if it is `true`, they should not run the callback, per
  // the API guarantees above.
  scoped_refptr<CancellationFlag> cancel_requests_;
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_PUBLIC_CPP_DATA_DECODER_H_
