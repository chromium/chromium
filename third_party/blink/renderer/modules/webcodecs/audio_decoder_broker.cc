// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_decoder_broker.h"

#include <limits>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "build/buildflag.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_factory.h"
#include "media/base/media_log.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/clients/mojo_decoder_factory.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/renderers/default_decoder_factory.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/webcodecs/decoder_selector.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_mojo.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using DecoderDetails = blink::AudioDecoderBroker::DecoderDetails;

namespace WTF {

template <>
struct CrossThreadCopier<media::AudioDecoderConfig>
    : public CrossThreadCopierPassThrough<media::AudioDecoderConfig> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<media::DecoderStatus>
    : public CrossThreadCopierPassThrough<media::DecoderStatus> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<std::optional<DecoderDetails>>
    : public CrossThreadCopierPassThrough<std::optional<DecoderDetails>> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {

// Wrapper class for state and API calls that must be made from the
// |media_task_runner_|. Construction must happen on blink main thread to safely
// make use of ExecutionContext and Document. These GC blink types must not be
// stored/referenced by any other method.
class MediaAudioTaskWrapper {
 public:
  using CrossThreadOnceInitCB =
      WTF::CrossThreadOnceFunction<void(media::DecoderStatus status,
                                        std::optional<DecoderDetails>)>;
  using CrossThreadOnceDecodeCB =
      WTF::CrossThreadOnceFunction<void(media::DecoderStatus)>;
  using CrossThreadOnceResetCB = WTF::CrossThreadOnceClosure;

  MediaAudioTaskWrapper(
      base::WeakPtr<CrossThreadAudioDecoderClient> weak_client,
      ExecutionContext& execution_context,
      std::unique_ptr<media::MediaLog> media_log,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner,
      scoped_refptr<base::SequencedTaskRunner> main_task_runner)
      : weak_client_(std::move(weak_client)),
        media_task_runner_(std::move(media_task_runner)),
        main_task_runner_(std::move(main_task_runner)),
        media_log_(std::move(media_log)) {
    DVLOG(2) << __func__;
    DETACH_FROM_SEQUENCE(sequence_checker_);

    // TODO(chcunningham): set_disconnect_handler?
    // Mojo connection setup must occur here on the main thread where its safe
    // to use |execution_context| APIs.
    mojo::PendingRemote<media::mojom::InterfaceFactory> media_interface_factory;
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        media_interface_factory.InitWithNewPipeAndPassReceiver());

    // Mojo remote must be bound on media thread where it will be used.
    //|Unretained| is safe because |this| must be destroyed on the media task
    // runner.
    PostCrossThreadTask(
        *media_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(&MediaAudioTaskWrapper::BindOnTaskRunner,
                                 WTF::CrossThreadUnretained(this),
                                 std::move(media_interface_factory)));
  }

  virtual ~MediaAudioTaskWrapper() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  MediaAudioTaskWrapper(const MediaAudioTaskWrapper&) = delete;
  MediaAudioTaskWrapper& operator=(const MediaAudioTaskWrapper&) = delete;

  void Initialize(const media::AudioDecoderConfig& config) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    selector_ = std::make_unique<WebCodecsAudioDecoderSelector>(
        media_task_runner_,
        // TODO(chcunningham): Its ugly that we don't use a WeakPtr here, but
        // its not possible because the callback returns non-void. It happens
        // to be safe given the way the callback is called (never posted), but
        // we should refactor the return to be an out-param so we can be
        // consistent in using weak pointers.
        WTF::BindRepeating(&MediaAudioTaskWrapper::OnCreateDecoders,
                           WTF::Unretained(this)),
        WTF::BindRepeating(&MediaAudioTaskWrapper::OnDecodeOutput,
                           weak_factory_.GetWeakPtr()));

    selector_->SelectDecoder(
        config, /*low_delay=*/false,
        WTF::BindOnce(&MediaAudioTaskWrapper::OnDecoderSelected,
                      weak_factory_.GetWeakPtr()));
  }

  void Decode(scoped_refptr<media::DecoderBuffer> buffer, int cb_id) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!decoder_) {
      OnDecodeDone(cb_id, media::DecoderStatus::Codes::kNotInitialized);
      return;
    }

    decoder_->Decode(std::move(buffer),
                     WTF::BindOnce(&MediaAudioTaskWrapper::OnDecodeDone,
                                   weak_factory_.GetWeakPtr(), cb_id));
  }

  void Reset(int cb_id) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!decoder_) {
      OnReset(cb_id);
      return;
    }

    decoder_->Reset(WTF::BindOnce(&MediaAudioTaskWrapper::OnReset,
                                  weak_factory_.GetWeakPtr(), cb_id));
  }

 private:
  void BindOnTaskRunner(
      mojo::PendingRemote<media::mojom::InterfaceFactory> interface_factory) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    media_interface_factory_.Bind(std::move(interface_factory));

    // Bind the |interface_factory_| above before passing to
    // |external_decoder_factory|.
    std::unique_ptr<media::DecoderFactory> external_decoder_factory;
#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
    external_decoder_factory = std::make_unique<media::MojoDecoderFactory>(
        media_interface_factory_.get());
#endif
    decoder_factory_ = std::make_unique<media::DefaultDecoderFactory>(
        std::move(external_decoder_factory));
  }

  std::vector<std::unique_ptr<media::AudioDecoder>> OnCreateDecoders() {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    std::vector<std::unique_ptr<media::AudioDecoder>> audio_decoders;
    decoder_factory_->CreateAudioDecoders(media_task_runner_, media_log_.get(),
                                          &audio_decoders);

    return audio_decoders;
  }

  void OnDecoderSelected(std::unique_ptr<media::AudioDecoder> decoder) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // We're done with it.
    DCHECK(selector_);
    selector_.reset();

    decoder_ = std::move(decoder);

    media::DecoderStatus status = media::DecoderStatus::Codes::kOk;
    std::optional<DecoderDetails> decoder_details = std::nullopt;
    if (decoder_) {
      decoder_details = DecoderDetails({decoder_->GetDecoderType(),
                                        decoder_->IsPlatformDecoder(),
                                        decoder_->NeedsBitstreamConversion()});
    } else {
      status = media::DecoderStatus::Codes::kUnsupportedConfig;
    }

    // Fire |init_cb|.
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(&CrossThreadAudioDecoderClient::OnInitialize,
                                 weak_client_, status, decoder_details));
  }

  void OnDecodeOutput(scoped_refptr<media::AudioBuffer> buffer) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(&CrossThreadAudioDecoderClient::OnDecodeOutput,
                                 weak_client_, std::move(buffer)));
  }

  void OnDecodeDone(int cb_id, media::DecoderStatus status) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(&CrossThreadAudioDecoderClient::OnDecodeDone,
                                 weak_client_, cb_id, std::move(status)));
  }

  void OnReset(int cb_id) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(&CrossThreadAudioDecoderClient::OnReset,
                                 weak_client_, cb_id));
  }

  base::WeakPtr<CrossThreadAudioDecoderClient> weak_client_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  mojo::Remote<media::mojom::InterfaceFactory> media_interface_factory_;
  std::unique_ptr<WebCodecsAudioDecoderSelector> selector_;
  std::unique_ptr<media::DefaultDecoderFactory> decoder_factory_;
  std::unique_ptr<media::AudioDecoder> decoder_;
  gfx::ColorSpace target_color_space_;

  std::unique_ptr<media::MediaLog> media_log_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Using unretained for decoder/selector callbacks is generally not safe /
  // fragile. Some decoders (e.g. those that offload) will call the output
  // callback after destruction.
  base::WeakPtrFactory<MediaAudioTaskWrapper> weak_factory_{this};
};

AudioDecoderBroker::AudioDecoderBroker(media::MediaLog* media_log,
                                       ExecutionContext& execution_context)
    // Use a worker task runner to avoid scheduling decoder
    // work on the main thread.
    : media_task_runner_(worker_pool::CreateSequencedTaskRunner({})) {
  DVLOG(2) << __func__;
  media_tasks_ = std::make_unique<MediaAudioTaskWrapper>(
      weak_factory_.GetWeakPtr(), execution_context, media_log->Clone(),
      media_task_runner_,
      execution_context.GetTaskRunner(TaskType::kInternalMedia));
}

AudioDecoderBroker::~AudioDecoderBroker() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  media_task_runner_->DeleteSoon(FROM_HERE, std::move(media_tasks_));
}

media::AudioDecoderType AudioDecoderBroker::GetDecoderType() const {
  return decoder_details_ ? decoder_details_->decoder_type
                          : media::AudioDecoderType::kBroker;
}

bool AudioDecoderBroker::IsPlatformDecoder() const {
  return decoder_details_ ? decoder_details_->is_platform_decoder : false;
}

void AudioDecoderBroker::Initialize(const media::AudioDecoderConfig& config,
                                    media::CdmContext* cdm_context,
                                    InitCB init_cb,
                                    const OutputCB& output_cb,
                                    const media::WaitingCB& waiting_cb) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!init_cb_) << "Initialize already pending";

  // The following are not currently supported in WebCodecs.
  DCHECK(!cdm_context);
  DCHECK(!waiting_cb);

  init_cb_ = std::move(init_cb);
  output_cb_ = output_cb;

  // Clear details from previously initialized decoder. New values will arrive
  // via OnInitialize().
  decoder_details_.reset();

  PostCrossThreadTask(
      *media_task_runner_, FROM_HERE,
      WTF::CrossThreadBindOnce(&MediaAudioTaskWrapper::Initialize,
                               WTF::CrossThreadUnretained(media_tasks_.get()),
                               config));
}

int AudioDecoderBroker::CreateCallbackId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // 0 and -1 are reserved by wtf::HashMap ("empty" and "deleted").
  while (++last_callback_id_ == 0 ||
         last_callback_id_ == std::numeric_limits<uint32_t>::max() ||
         pending_decode_cb_map_.Contains(last_callback_id_) ||
         pending_reset_cb_map_.Contains(last_callback_id_))
    ;

  return last_callback_id_;
}

void AudioDecoderBroker::OnInitialize(media::DecoderStatus status,
                                      std::optional<DecoderDetails> details) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  decoder_details_ = details;
  std::move(init_cb_).Run(status);
}

void AudioDecoderBroker::Decode(scoped_refptr<media::DecoderBuffer> buffer,
                                DecodeCB decode_cb) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int callback_id = CreateCallbackId();
  pending_decode_cb_map_.insert(callback_id, std::move(decode_cb));

  PostCrossThreadTask(
      *media_task_runner_, FROM_HERE,
      WTF::CrossThreadBindOnce(&MediaAudioTaskWrapper::Decode,
                               WTF::CrossThreadUnretained(media_tasks_.get()),
                               buffer, callback_id));
}

void AudioDecoderBroker::OnDecodeDone(int cb_id, media::DecoderStatus status) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_decode_cb_map_.Contains(cb_id));

  auto iter = pending_decode_cb_map_.find(cb_id);
  DecodeCB decode_cb = std::move(iter->value);
  pending_decode_cb_map_.erase(cb_id);

  // Do this last. Caller may destruct |this| in response to the callback while
  // this method is still on the stack.
  std::move(decode_cb).Run(std::move(status));
}

void AudioDecoderBroker::Reset(base::OnceClosure reset_cb) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int callback_id = CreateCallbackId();
  pending_reset_cb_map_.insert(callback_id, std::move(reset_cb));

  PostCrossThreadTask(
      *media_task_runner_, FROM_HERE,
      WTF::CrossThreadBindOnce(&MediaAudioTaskWrapper::Reset,
                               WTF::CrossThreadUnretained(media_tasks_.get()),
                               callback_id));
}

bool AudioDecoderBroker::NeedsBitstreamConversion() const {
  // No known scenarios where this is needed by WebCodecs. See
  // https://crbug.com/1119947
  DCHECK(!decoder_details_ || !decoder_details_->needs_bitstream_conversion);

  return decoder_details_ ? decoder_details_->needs_bitstream_conversion
                          : false;
}

void AudioDecoderBroker::OnReset(int cb_id) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_reset_cb_map_.Contains(cb_id));

  auto iter = pending_reset_cb_map_.find(cb_id);
  base::OnceClosure reset_cb = std::move(iter->value);
  pending_reset_cb_map_.erase(cb_id);

  // Do this last. Caller may destruct |this| in response to the callback while
  // this method is still on the stack.
  std::move(reset_cb).Run();
}

void AudioDecoderBroker::OnDecodeOutput(
    scoped_refptr<media::AudioBuffer> buffer) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(output_cb_);

  output_cb_.Run(std::move(buffer));
}

}  // namespace blink
