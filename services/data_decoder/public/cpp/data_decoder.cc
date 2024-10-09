// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/data_decoder.h"

#include <memory>
#include <string>
#include <utility>

#include "base/features.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/rust_buildflags.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "components/facilitated_payments/core/mojom/pix_code_validator.mojom.h"
#include "net/http/structured_headers.h"
#include "services/data_decoder/public/mojom/cbor_parser.mojom.h"
#include "services/data_decoder/public/mojom/gzipper.mojom.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"
#include "services/data_decoder/public/mojom/structured_headers_parser.mojom.h"
#include "services/data_decoder/public/mojom/xml_parser.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/types/expected_macros.h"
#include "services/data_decoder/public/cpp/json_sanitizer.h"
#endif

#if !BUILDFLAG(USE_BLINK)
#include "services/data_decoder/data_decoder_service.h"  // nogncheck
#endif

namespace data_decoder {

namespace {

// The default amount of idle time to tolerate on a DataDecoder instance. If the
// instance is unused for this period of time, the underlying service process
// (if any) may be killed and only restarted once needed again.
// On platforms (like iOS) or environments (like some unit tests) where
// out-of-process services are not used, this has no effect.
constexpr base::TimeDelta kServiceProcessIdleTimeoutDefault{base::Seconds(5)};

// Encapsulates an in-process data decoder parsing request. This provides shared
// ownership of the caller's callback so that it may be invoked exactly once by
// *either* the successful response handler *or* the parser's disconnection
// handler. This also owns a Remote<T> which is kept alive for the duration of
// the request.
template <typename T, typename V>
class ValueParseRequest : public base::RefCounted<ValueParseRequest<T, V>> {
 public:
  ValueParseRequest(DataDecoder::ResultCallback<V> callback,
                    scoped_refptr<DataDecoder::CancellationFlag> is_cancelled)
      : callback_(std::move(callback)), is_cancelled_(is_cancelled) {}

  ValueParseRequest(const ValueParseRequest&) = delete;
  ValueParseRequest& operator=(const ValueParseRequest&) = delete;

  mojo::Remote<T>& remote() { return remote_; }
  DataDecoder::ResultCallback<V>& callback() { return callback_; }

  // Creates a pipe and binds it to the remote(), and sets up the
  // disconnect handler to invoke callback() with an error.
  mojo::PendingReceiver<T> BindRemote() {
    auto receiver = remote_.BindNewPipeAndPassReceiver();
    remote_.set_disconnect_handler(
        base::BindOnce(&ValueParseRequest::OnRemoteDisconnected, this));
    return receiver;
  }

  void OnServiceValue(std::optional<V> value) {
    OnServiceValueOrError(std::move(value), std::nullopt);
  }

  // Handles a successful parse from the service.
  void OnServiceValueOrError(std::optional<V> value,
                             const std::optional<std::string>& error) {
    if (!callback() || is_cancelled_->data) {
      return;
    }

    base::expected<V, std::string> result;
    if (value) {
      result = std::move(*value);
    } else {
      result = base::unexpected(error.value_or("unknown error"));
    }

    // Copy the callback onto the stack before resetting the Remote, as that may
    // delete |this|.
    auto local_callback = std::move(callback());

    // Reset the |remote_| since we aren't using it again and we don't want it
    // to trip the disconnect handler. May delete |this|.
    remote_.reset();

    // We run the callback after reset just in case it does anything funky like
    // spin a nested RunLoop.
    std::move(local_callback).Run(std::move(result));
  }

 private:
  friend class base::RefCounted<ValueParseRequest>;

  ~ValueParseRequest() = default;

  void OnRemoteDisconnected() {
    if (is_cancelled_->data) {
      return;
    }

    if (callback()) {
      std::move(callback())
          .Run(base::unexpected("Data Decoder terminated unexpectedly"));
    }
  }

  mojo::Remote<T> remote_;
  DataDecoder::ResultCallback<V> callback_;
  scoped_refptr<DataDecoder::CancellationFlag> is_cancelled_;
};

#if !BUILDFLAG(USE_BLINK)
void BindInProcessService(
    mojo::PendingReceiver<mojom::DataDecoderService> receiver) {
  static base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>>
      task_runner{base::ThreadPool::CreateSequencedTaskRunner({})};
  if (!(*task_runner)->RunsTasksInCurrentSequence()) {
    (*task_runner)
        ->PostTask(FROM_HERE,
                   base::BindOnce(&BindInProcessService, std::move(receiver)));
    return;
  }

  static base::NoDestructor<DataDecoderService> service;
  service->BindReceiver(std::move(receiver));
}
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(BUILD_RUST_JSON_READER)

void ParsingComplete(scoped_refptr<DataDecoder::CancellationFlag> is_cancelled,
                     DataDecoder::ValueParseCallback callback,
                     base::JSONReader::Result value_with_error) {
  if (is_cancelled->data) {
    return;
  }

  if (!value_with_error.has_value()) {
    std::move(callback).Run(base::unexpected(value_with_error.error().message));
  } else {
    std::move(callback).Run(std::move(*value_with_error));
  }
}

#endif

}  // namespace

DataDecoder::DataDecoder() : DataDecoder(kServiceProcessIdleTimeoutDefault) {}

DataDecoder::DataDecoder(base::TimeDelta idle_timeout)
    : idle_timeout_(idle_timeout),
      cancel_requests_(new CancellationFlag(false)) {}

DataDecoder::~DataDecoder() {
  cancel_requests_->data = true;
}

mojom::DataDecoderService* DataDecoder::GetService() {
  // Lazily start an instance of the service if possible and necessary.
  if (!service_) {
    auto* provider = ServiceProvider::Get();
    if (provider) {
      provider->BindDataDecoderService(service_.BindNewPipeAndPassReceiver());
    } else {
#if !BUILDFLAG(USE_BLINK)
      BindInProcessService(service_.BindNewPipeAndPassReceiver());
#else
      LOG(FATAL) << "data_decoder::ServiceProvider::Set() must be called "
                 << "before any instances of DataDecoder can be used.";
#endif
    }

    service_.reset_on_disconnect();
    service_.reset_on_idle_timeout(idle_timeout_);
  }

  return service_.get();
}

void DataDecoder::ParseJson(const std::string& json,
                            ValueParseCallback callback) {
  // Measure decoding time by intercepting the callback.
  callback = base::BindOnce(
      [](base::ElapsedTimer timer, ValueParseCallback callback,
         base::expected<base::Value, std::string> result) {
        base::UmaHistogramTimes("Security.DataDecoder.Json.DecodingTime",
                                timer.Elapsed());
        std::move(callback).Run(std::move(result));
      },
      base::ElapsedTimer(), std::move(callback));

  if (base::JSONReader::UsingRust()) {
#if BUILDFLAG(BUILD_RUST_JSON_READER)
    if (base::features::kUseRustJsonParserInCurrentSequence.Get()) {
      base::JSONReader::Result result =
          base::JSONReader::ReadAndReturnValueWithError(json,
                                                        base::JSON_PARSE_RFC);
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&ParsingComplete, cancel_requests_,
                                    std::move(callback), std::move(result)));
    } else {
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::TaskPriority::USER_VISIBLE},
          base::BindOnce(
              [](const std::string& json) {
                return base::JSONReader::ReadAndReturnValueWithError(
                    json, base::JSON_PARSE_RFC);
              },
              json),
          base::BindOnce(&ParsingComplete, cancel_requests_,
                         std::move(callback)));
    }
#else   // BUILDFLAG(BUILD_RUST_JSON_READER)
    CHECK(false)
        << "UseJsonParserFeature enabled, but not supported in this build.";
#endif  // BUILDFLAG(BUILD_RUST_JSON_READER)
    return;
  }

#if BUILDFLAG(IS_ANDROID)
  // For Android, if the full Rust parser is not available, we use the
  // in-process sanitizer and then parse in-process.
  JsonSanitizer::Sanitize(
      json, base::BindOnce(
                [](ValueParseCallback callback,
                   scoped_refptr<CancellationFlag> is_cancelled,
                   JsonSanitizer::Result result) {
                  if (is_cancelled->data) {
                    return;
                  }

                  RETURN_IF_ERROR(result, [&](std::string error) {
                    std::move(callback).Run(base::unexpected(std::move(error)));
                  });

                  ParsingComplete(is_cancelled, std::move(callback),
                                  base::JSONReader::ReadAndReturnValueWithError(
                                      result.value(), base::JSON_PARSE_RFC));
                },
                std::move(callback), cancel_requests_));
#else   // BUILDFLAG(IS_ANDROID)
  // Parse JSON out-of-process.
  auto request =
      base::MakeRefCounted<ValueParseRequest<mojom::JsonParser, base::Value>>(
          std::move(callback), cancel_requests_);
  GetService()->BindJsonParser(request->BindRemote());
  request->remote()->Parse(
      json, base::JSON_PARSE_RFC,
      base::BindOnce(&ValueParseRequest<mojom::JsonParser,
                                        base::Value>::OnServiceValueOrError,
                     request));
#endif  // BUILDFLAG(IS_ANDROID)
}

// static
void DataDecoder::ParseJsonIsolated(const std::string& json,
                                    ValueParseCallback callback) {
  auto decoder = std::make_unique<DataDecoder>();
  auto* raw_decoder = decoder.get();

  // We bind the DataDecoder's ownership into the result callback to ensure that
  // it stays alive until the operation is complete.
  raw_decoder->ParseJson(
      json, base::BindOnce(
                [](std::unique_ptr<DataDecoder>, ValueParseCallback callback,
                   ValueOrError result) {
                  std::move(callback).Run(std::move(result));
                },
                std::move(decoder), std::move(callback)));
}

void DataDecoder::ParseStructuredHeaderItem(
    const std::string& header,
    StructuredHeaderParseItemCallback callback) {
  auto request = base::MakeRefCounted<
      ValueParseRequest<mojom::StructuredHeadersParser,
                        net::structured_headers::ParameterizedItem>>(
      std::move(callback), cancel_requests_);
  GetService()->BindStructuredHeadersParser(request->BindRemote());
  request->remote()->ParseItem(
      header,
      base::BindOnce(
          &ValueParseRequest<
              mojom::StructuredHeadersParser,
              net::structured_headers::ParameterizedItem>::OnServiceValue,
          request));
}

// static
void DataDecoder::ParseStructuredHeaderItemIsolated(
    const std::string& header,
    StructuredHeaderParseItemCallback callback) {
  auto decoder = std::make_unique<DataDecoder>();
  auto* raw_decoder = decoder.get();

  // We bind the DataDecoder's ownership into the result callback to ensure that
  // it stays alive until the operation is complete.
  raw_decoder->ParseStructuredHeaderItem(
      header, base::BindOnce(
                  [](std::unique_ptr<DataDecoder>,
                     StructuredHeaderParseItemCallback callback,
                     base::expected<net::structured_headers::ParameterizedItem,
                                    std::string> result) {
                    std::move(callback).Run(std::move(result));
                  },
                  std::move(decoder), std::move(callback)));
}

void DataDecoder::ParseStructuredHeaderList(
    const std::string& header,
    StructuredHeaderParseListCallback callback) {
  auto request =
      base::MakeRefCounted<ValueParseRequest<mojom::StructuredHeadersParser,
                                             net::structured_headers::List>>(
          std::move(callback), cancel_requests_);
  GetService()->BindStructuredHeadersParser(request->BindRemote());
  request->remote()->ParseList(
      header,
      base::BindOnce(
          &ValueParseRequest<mojom::StructuredHeadersParser,
                             net::structured_headers::List>::OnServiceValue,
          request));
}

// static
void DataDecoder::ParseStructuredHeaderListIsolated(
    const std::string& header,
    StructuredHeaderParseListCallback callback) {
  auto decoder = std::make_unique<DataDecoder>();
  auto* raw_decoder = decoder.get();

  // We bind the DataDecoder's ownership into the result callback to ensure that
  // it stays alive until the operation is complete.
  raw_decoder->ParseStructuredHeaderList(
      header,
      base::BindOnce(
          [](std::unique_ptr<DataDecoder>,
             StructuredHeaderParseListCallback callback,
             base::expected<net::structured_headers::List, std::string>
                 result) { std::move(callback).Run(std::move(result)); },
          std::move(decoder), std::move(callback)));
}

void DataDecoder::ParseStructuredHeaderDictionary(
    const std::string& header,
    StructuredHeaderParseDictionaryCallback callback) {
  auto request = base::MakeRefCounted<ValueParseRequest<
      mojom::StructuredHeadersParser, net::structured_headers::Dictionary>>(
      std::move(callback), cancel_requests_);
  GetService()->BindStructuredHeadersParser(request->BindRemote());
  request->remote()->ParseDictionary(
      header,
      base::BindOnce(&ValueParseRequest<
                         mojom::StructuredHeadersParser,
                         net::structured_headers::Dictionary>::OnServiceValue,
                     request));
}

// static
void DataDecoder::ParseStructuredHeaderDictionaryIsolated(
    const std::string& header,
    StructuredHeaderParseDictionaryCallback callback) {
  auto decoder = std::make_unique<DataDecoder>();
  auto* raw_decoder = decoder.get();

  // We bind the DataDecoder's ownership into the result callback to ensure that
  // it stays alive until the operation is complete.
  raw_decoder->ParseStructuredHeaderDictionary(
      header,
      base::BindOnce(
          [](std::unique_ptr<DataDecoder>,
             StructuredHeaderParseDictionaryCallback callback,
             base::expected<net::structured_headers::Dictionary, std::string>
                 result) { std::move(callback).Run(std::move(result)); },
          std::move(decoder), std::move(callback)));
}

void DataDecoder::ParseXml(
    const std::string& xml,
    mojom::XmlParser::WhitespaceBehavior whitespace_behavior,
    ValueParseCallback callback) {
  auto request =
      base::MakeRefCounted<ValueParseRequest<mojom::XmlParser, base::Value>>(
          std::move(callback), cancel_requests_);
  GetService()->BindXmlParser(request->BindRemote());
  request->remote()->Parse(
      xml, whitespace_behavior,
      base::BindOnce(&ValueParseRequest<mojom::XmlParser,
                                        base::Value>::OnServiceValueOrError,
                     request));
}

// static
void DataDecoder::ParseXmlIsolated(
    const std::string& xml,
    mojom::XmlParser::WhitespaceBehavior whitespace_behavior,
    ValueParseCallback callback) {
  auto decoder = std::make_unique<DataDecoder>();
  auto* raw_decoder = decoder.get();

  // We bind the DataDecoder's ownership into the result callback to ensure that
  // it stays alive until the operation is complete.
  raw_decoder->ParseXml(
      xml, whitespace_behavior,
      base::BindOnce(
          [](std::unique_ptr<DataDecoder>, ValueParseCallback callback,
             ValueOrError result) {
            std::move(callback).Run(std::move(result));
          },
          std::move(decoder), std::move(callback)));
}

void DataDecoder::Deflate(base::span<const uint8_t> data,
                          GzipperCallback callback) {
  auto request = base::MakeRefCounted<
      ValueParseRequest<mojom::Gzipper, mojo_base::BigBuffer>>(
      std::move(callback), cancel_requests_);
  GetService()->BindGzipper(request->BindRemote());
  request->remote()->Deflate(
      data,
      base::BindOnce(&ValueParseRequest<mojom::Gzipper,
                                        mojo_base::BigBuffer>::OnServiceValue,
                     request));
}

void DataDecoder::Inflate(base::span<const uint8_t> data,
                          uint64_t max_uncompressed_size,
                          GzipperCallback callback) {
  auto request = base::MakeRefCounted<
      ValueParseRequest<mojom::Gzipper, mojo_base::BigBuffer>>(
      std::move(callback), cancel_requests_);
  GetService()->BindGzipper(request->BindRemote());
  request->remote()->Inflate(
      data, max_uncompressed_size,
      base::BindOnce(&ValueParseRequest<mojom::Gzipper,
                                        mojo_base::BigBuffer>::OnServiceValue,
                     request));
}

void DataDecoder::GzipCompress(base::span<const uint8_t> data,
                               GzipperCallback callback) {
  auto request = base::MakeRefCounted<
      ValueParseRequest<mojom::Gzipper, mojo_base::BigBuffer>>(
      std::move(callback), cancel_requests_);
  GetService()->BindGzipper(request->BindRemote());
  request->remote()->Compress(
      data,
      base::BindOnce(&ValueParseRequest<mojom::Gzipper,
                                        mojo_base::BigBuffer>::OnServiceValue,
                     request));
}

void DataDecoder::GzipUncompress(base::span<const uint8_t> data,
                                 GzipperCallback callback) {
  auto request = base::MakeRefCounted<
      ValueParseRequest<mojom::Gzipper, mojo_base::BigBuffer>>(
      std::move(callback), cancel_requests_);
  GetService()->BindGzipper(request->BindRemote());
  request->remote()->Uncompress(
      data,
      base::BindOnce(&ValueParseRequest<mojom::Gzipper,
                                        mojo_base::BigBuffer>::OnServiceValue,
                     request));
}

void DataDecoder::ParseCbor(base::span<const uint8_t> data,
                            ValueParseCallback callback) {
  auto request =
      base::MakeRefCounted<ValueParseRequest<mojom::CborParser, base::Value>>(
          std::move(callback), cancel_requests_);
  GetService()->BindCborParser(request->BindRemote());
  request->remote()->Parse(
      data,
      base::BindOnce(&ValueParseRequest<mojom::CborParser,
                                        base::Value>::OnServiceValueOrError,
                     request));
}

// static
void DataDecoder::ParseCborIsolated(base::span<const uint8_t> data,
                                    ValueParseCallback callback) {
  auto decoder = std::make_unique<DataDecoder>();
  auto* raw_decoder = decoder.get();

  // We bind the DataDecoder's ownership into the result callback to ensure that
  // it stays alive until the operation is complete.
  raw_decoder->ParseCbor(
      data, base::BindOnce(
                [](std::unique_ptr<DataDecoder>, ValueParseCallback callback,
                   ValueOrError result) {
                  std::move(callback).Run(std::move(result));
                },
                std::move(decoder), std::move(callback)));
}

void DataDecoder::ValidatePixCode(const std::string& pix_code,
                                  ValidationCallback callback) {
  auto request = base::MakeRefCounted<
      ValueParseRequest<payments::facilitated::mojom::PixCodeValidator, bool>>(
      std::move(callback), cancel_requests_);
  GetService()->BindPixCodeValidator(request->BindRemote());
  request->remote()->ValidatePixCode(
      pix_code,
      base::BindOnce(
          &ValueParseRequest<payments::facilitated::mojom::PixCodeValidator,
                             bool>::OnServiceValue,
          request));
}

}  // namespace data_decoder
