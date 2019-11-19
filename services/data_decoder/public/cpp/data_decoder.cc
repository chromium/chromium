// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/data_decoder.h"

#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"
#include "services/data_decoder/public/mojom/xml_parser.mojom.h"

#if defined(OS_ANDROID)
#include "base/json/json_reader.h"
#include "services/data_decoder/public/cpp/json_sanitizer.h"
#endif

#if defined(OS_IOS)
#include "base/task/post_task.h"
#include "services/data_decoder/data_decoder_service.h"  // nogncheck
#endif

namespace data_decoder {

namespace {

// The amount of idle time to tolerate on a DataDecoder instance. If the
// instance is unused for this period of time, the underlying service process
// (if any) may be killed and only restarted once needed again.
//
// On platforms (like iOS) or environments (like some unit tests) where
// out-of-process services are not used, this has no effect.
constexpr base::TimeDelta kServiceProcessIdleTimeout{
    base::TimeDelta::FromSeconds(5)};

// Encapsulates an in-process data decoder parsing request. This provides shared
// ownership of the caller's callback so that it may be invoked exactly once by
// *either* the successful response handler *or* the parsers's disconnection
// handler. This also owns a Remote<T> which is kept alive for the duration of
// the request.
template <typename T>
class ValueParseRequest : public base::RefCounted<ValueParseRequest<T>> {
 public:
  explicit ValueParseRequest(DataDecoder::ValueParseCallback callback)
      : callback_(std::move(callback)) {}

  mojo::Remote<T>& remote() { return remote_; }
  DataDecoder::ValueParseCallback& callback() { return callback_; }

  // Creates a pipe and binds it to the remote(), and sets up the
  // disconnect handler to invoke callback() with an error.
  mojo::PendingReceiver<T> BindRemote() {
    auto receiver = remote_.BindNewPipeAndPassReceiver();
    remote_.set_disconnect_handler(
        base::BindOnce(&ValueParseRequest::OnRemoteDisconnected, this));
    return receiver;
  }

  // Handles a successful parse from the service.
  void OnServiceValueOrError(base::Optional<base::Value> value,
                             const base::Optional<std::string>& error) {
    if (!callback())
      return;

    DataDecoder::ValueOrError result;
    if (value)
      result.value = std::move(value);
    else
      result.error = error.value_or("unknown error");

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
    if (callback()) {
      std::move(callback())
          .Run(DataDecoder::ValueOrError::Error(
              "Data Decoder terminated unexpectedly"));
    }
  }

  mojo::Remote<T> remote_;
  DataDecoder::ValueParseCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ValueParseRequest);
};

#if defined(OS_IOS)
void BindInProcessService(
    mojo::PendingReceiver<mojom::DataDecoderService> receiver) {
  static base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>>
      task_runner{base::CreateSequencedTaskRunner({base::ThreadPool()})};
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

}  // namespace

DataDecoder::ValueOrError::ValueOrError() = default;

DataDecoder::ValueOrError::ValueOrError(ValueOrError&&) = default;

DataDecoder::ValueOrError::~ValueOrError() = default;

// static
DataDecoder::ValueOrError DataDecoder::ValueOrError::Value(base::Value value) {
  ValueOrError result;
  result.value = std::move(value);
  return result;
}

DataDecoder::ValueOrError DataDecoder::ValueOrError::Error(
    const std::string& error) {
  ValueOrError result;
  result.error = error;
  return result;
}

DataDecoder::DataDecoder() = default;

DataDecoder::~DataDecoder() = default;

mojom::DataDecoderService* DataDecoder::GetService() {
  // Lazily start an instance of the service if possible and necessary.
  if (!service_) {
    auto* provider = ServiceProvider::Get();
    if (provider) {
      provider->BindDataDecoderService(service_.BindNewPipeAndPassReceiver());
    } else {
#if defined(OS_IOS)
      BindInProcessService(service_.BindNewPipeAndPassReceiver());
#else
      LOG(FATAL) << "data_decoder::ServiceProvider::Set() must be called "
                 << "before any instances of DataDecoder can be used.";
      return nullptr;
#endif
    }

    service_.reset_on_disconnect();
    service_.reset_on_idle_timeout(kServiceProcessIdleTimeout);
  }

  return service_.get();
}

void DataDecoder::ParseJson(const std::string& json,
                            ValueParseCallback callback) {
#if defined(OS_ANDROID)
  // For Android, we use the in-process sanitizer and then parse with a simple
  // JSONReader.
  JsonSanitizer::Sanitize(
      json, base::BindOnce(
                [](ValueParseCallback callback, JsonSanitizer::Result result) {
                  if (!result.value) {
                    std::move(callback).Run(ValueOrError::Error(*result.error));
                    return;
                  }

                  base::JSONReader::ValueWithError value_with_error =
                      base::JSONReader::ReadAndReturnValueWithError(
                          *result.value, base::JSON_PARSE_RFC);
                  if (!value_with_error.value) {
                    std::move(callback).Run(
                        ValueOrError::Error(value_with_error.error_message));
                    return;
                  }

                  std::move(callback).Run(
                      ValueOrError::Value(std::move(*value_with_error.value)));
                },
                std::move(callback)));
#else
  auto request = base::MakeRefCounted<ValueParseRequest<mojom::JsonParser>>(
      std::move(callback));
  GetService()->BindJsonParser(request->BindRemote());
  request->remote()->Parse(
      json, base::BindOnce(
                &ValueParseRequest<mojom::JsonParser>::OnServiceValueOrError,
                request));
#endif
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

void DataDecoder::ParseXml(const std::string& xml,
                           ValueParseCallback callback) {
  auto request = base::MakeRefCounted<ValueParseRequest<mojom::XmlParser>>(
      std::move(callback));
  GetService()->BindXmlParser(request->BindRemote());
  request->remote()->Parse(
      xml, base::BindOnce(
               &ValueParseRequest<mojom::XmlParser>::OnServiceValueOrError,
               request));
}

// static
void DataDecoder::ParseXmlIsolated(const std::string& xml,
                                   ValueParseCallback callback) {
  auto decoder = std::make_unique<DataDecoder>();
  auto* raw_decoder = decoder.get();

  // We bind the DataDecoder's ownership into the result callback to ensure that
  // it stays alive until the operation is complete.
  raw_decoder->ParseXml(
      xml, base::BindOnce(
               [](std::unique_ptr<DataDecoder>, ValueParseCallback callback,
                  ValueOrError result) {
                 std::move(callback).Run(std::move(result));
               },
               std::move(decoder), std::move(callback)));
}

}  // namespace data_decoder
