// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_PUBLIC_CPP_DATA_DECODER_H_
#define SERVICES_DATA_DECODER_PUBLIC_CPP_DATA_DECODER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/service_provider.h"
#include "services/data_decoder/public/mojom/data_decoder_service.mojom.h"

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
  DataDecoder();
  ~DataDecoder();

  // The result of a Parse*() call that can return either a Value or an error
  // string. Exactly one of either |value| or |error| will have a value when
  // returned by either operation.
  struct ValueOrError {
    ValueOrError();
    ValueOrError(ValueOrError&&);
    ~ValueOrError();

    static ValueOrError Value(base::Value value);
    static ValueOrError Error(const std::string& error);

    base::Optional<base::Value> value;
    base::Optional<std::string> error;
  };

  using ValueParseCallback = base::OnceCallback<void(ValueOrError)>;

  // Returns a raw interface to the service instance. This launches an instance
  // of the service process if possible on the current platform, or returns a
  // connection to the in-process instance of in a test environment using
  // InProcessDataDecoderService.
  mojom::DataDecoderService* GetService();

  // Parses the potentially unsafe JSON string in |json| using this
  // DataDecoder's service instance or some other platform-specific decoding
  // facility.
  //
  // Note that |callback| will only be called if the parsing operation succeeds
  // or fails before this DataDecoder is destroyed.
  void ParseJson(const std::string& json, ValueParseCallback callback);

  // Parses the potentially unsafe JSON string in |json|. This static helper
  // uses a dedicated instance of the Data Decoder service on applicable
  // platforms.
  static void ParseJsonIsolated(const std::string& json,
                                ValueParseCallback callback);

  // Parses the potentially unsafe XML string in |xml| using this
  // DataDecoder's service instance. The Value provided to the callback
  // is a structured tree representing the XML document. See
  // ../mojom/xml_parser.mojom for details on the structure, and
  // safe_xml_parser.h for utilities to access parsed data.
  //
  // Note that |callback| will only be called if the parsing operation succeeds
  // or fails before this DataDecoder is destroyed.
  void ParseXml(const std::string& xml, ValueParseCallback callback);

  // Parses the potentially unsafe XML string in |xml|. This static helper
  // uses a dedicated instance of the Data Decoder service on applicable
  // platforms.
  static void ParseXmlIsolated(const std::string& xml,
                               ValueParseCallback callback);

 private:
  // This instance's connection to the service. This connection is lazily
  // established and may be reset after long periods of idle time.
  mojo::Remote<mojom::DataDecoderService> service_;

  DISALLOW_COPY_AND_ASSIGN(DataDecoder);
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_PUBLIC_CPP_DATA_DECODER_H_
