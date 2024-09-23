// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_decoder_broker.h"

#include <limits>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "build/buildflag.h"
#include "media/base/decoder_factory.h"
#include "media/base/decoder_status.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/clients/mojo_decoder_factory.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/renderers/default_decoder_factory.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/webcodecs/decoder_selector.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_mojo.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/color_space.h"

#if BUILDFLAG(IS_FUCHSIA)
#include "media/fuchsia/video/fuchsia_decoder_factory.h"
#endif

using DecoderDetails = blink::VideoDecoderBroker::DecoderDetails;

namespace WTF {

template <>
struct CrossThreadCopier<media::VideoDecoderConfig>
    : public CrossThreadCopierPassThrough<media::VideoDecoderConfig> {
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
class MediaVideoTaskWrapper {
 public:
  using CrossThreadOnceInitCB =
      WTF::CrossThreadOnceFunction<void(media::DecoderStatus status,
                                        std::optional<DecoderDetails>)>;
  using CrossThreadOnceDecodeCB =
      WTF::CrossThreadOnceFunction<void(const media::DecoderStatus&)>;
  using CrossThreadOnceResetCB = WTF::CrossThreadOnceClosure;

  MediaVideoTaskWrapper(
      base::WeakPtr<CrossThreadVideoDecoderClient> weak_client,
      ExecutionContext& execution_context,
      media::GpuVideoAcceleratorFactories* gpu_factories,
      std::unique_ptr<media::MediaLog> media_log,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner,
      scoped_refptr<base::SequencedTaskRunner> main_task_runner)
      : weak_client_(std::move(weak_client)),
        media_task_runner_(std::move(media_task_runner)),
        main_task_runner_(std::move(main_task_runner)),
        gpu_factories_(gpu_factories),
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
    // |Unretained| is safe because |this| must be destroyed on the media task
    // runner.
    PostCrossThreadTask(
        *media_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(&MediaVideoTaskWrapper::BindOnTaskRunner,
                                 WTF::CrossThreadUnretained(this),
                                 std::move(media_interface_factory)));

#if BUILDFLAG(IS_FUCHSIA)
    execution_context.GetBrowserInterfaceBroker().GetInterface(
        fuchsia_media_codec_provider_.InitWithNewPipeAndPassReceiver());
#endif

    // TODO(sandersd): Target color space is used by DXVA VDA to pick an
    // efficient conversion for FP16 HDR content, and for no other purpose.
    // For <video>, we use the document's colorspace, but for WebCodecs we can't
    // infer that frames will be rendered to a document (there might not even be
    // a document). If this is relevant for WebCodecs, we should make it a
    // configuration hint.
    target_color_space_ = gfx::ColorSpace::CreateSRGB();
  }

  virtual ~MediaVideoTaskWrapper() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  MediaVideoTaskWrapper(const MediaVideoTaskWrapper&) = delete;
  MediaVideoTaskWrapper& operator=(const MediaVideoTaskWrapper&) = delete;

  void Initialize(const media::VideoDecoderConfig& config, bool low_delay) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    selector_ = std::make_unique<WebCodecsVideoDecoderSelector>(
        media_task_runner_,
        // TODO(chcunningham): Its ugly that we don't use a WeakPtr here, but
        // its not possible because the callback returns non-void. It happens
        // to be safe given the way the callback is called (never posted), but
        // we should refactor the return to be an out-param so we can be
        // consistent in using weak pointers.
        WTF::BindRepeating(&MediaVideoTaskWrapper::OnCreateDecoders,
                           WTF::Unretained(this)),
        WTF::BindRepeating(&MediaVideoTaskWrapper::OnDecodeOutput,
                           weak_factory_.GetWeakPtr()));

    selector_->SelectDecoder(
        config, low_delay,
        WTF::BindOnce(&MediaVideoTaskWrapper::OnDecoderSelected,
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
                     WTF::BindOnce(&MediaVideoTaskWrapper::OnDecodeDone,
                                   weak_factory_.GetWeakPtr(), cb_id));
  }

  void Reset(int cb_id) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!decoder_) {
      OnReset(cb_id);
      return;
    }

    decoder_->Reset(WTF::BindOnce(&MediaVideoTaskWrapper::OnReset,
                                  weak_factory_.GetWeakPtr(), cb_id));
  }

  void UpdateHardwarePreference(HardwarePreference preference) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (hardware_preference_ != preference) {
      hardware_preference_ = preference;
      decoder_factory_needs_update_ = true;
    }
  }

 private:
  void BindOnTaskRunner(
      mojo::PendingRemote<media::mojom::InterfaceFactory> interface_factory) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    media_interface_factory_.Bind(std::move(interface_factory));
  }

  void UpdateDecoderFactory() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(decoder_factory_needs_update_);

    decoder_factory_needs_update_ = false;

    // Bind the |interface_factory_| above before passing to
    // |external_decoder_factory|.
    std::unique_ptr<media::DecoderFactory> external_decoder_factory;
    if (hardware_preference_ != HardwarePreference::kPreferSoftware) {
#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
      external_decoder_factory = std::make_unique<media::MojoDecoderFactory>(
          media_interface_factory_.get());
#elif BUILDFLAG(IS_FUCHSIA)
      DCHECK(fuchsia_media_codec_provider_);
      external_decoder_factory = std::make_unique<media::FuchsiaDecoderFactory>(
          std::move(fuchsia_media_codec_provider_),
          /*allow_overlays=*/false);
#endif
    }

    if (hardware_preference_ == HardwarePreference::kPreferHardware) {
      decoder_factory_ = std::move(external_decoder_factory);
      return;
    }

    decoder_factory_ = std::make_unique<media::DefaultDecoderFactory>(
        std::move(external_decoder_factory));
  }

  void OnRequestOverlayInfo(bool decoder_requires_restart_for_overlay,
                            media::ProvideOverlayInfoCB overlay_info_cb) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Android overlays are not supported.
    if (overlay_info_cb)
      std::move(overlay_info_cb).Run(media::OverlayInfo());
  }

  std::vector<std::unique_ptr<media::VideoDecoder>> OnCreateDecoders() {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (decoder_factory_needs_update_)
      UpdateDecoderFactory();

    std::vector<std::unique_ptr<media::VideoDecoder>> video_decoders;

    // We can end up with a null |decoder_factory_| if
    // |hardware_preference_| filtered out all available factories.
    if (decoder_factory_) {
      decoder_factory_->CreateVideoDecoders(
          media_task_runner_, gpu_factories_, media_log_.get(),
          WTF::BindRepeating(&MediaVideoTaskWrapper::OnRequestOverlayInfo,
                             weak_factory_.GetWeakPtr()),
          target_color_space_, &video_decoders);
    }

    return video_decoders;
  }

  void OnDecoderSelected(std::unique_ptr<media::VideoDecoder> decoder) {
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
                                        decoder_->NeedsBitstreamConversion(),
                                        decoder_->GetMaxDecodeRequests()});
    } else {
      status = media::DecoderStatus::Codes::kUnsupportedConfig;
    }

    // Fire |init_cb|.
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(&CrossThreadVideoDecoderClient::OnInitialize,
                                 weak_client_, status, decoder_details));
  }

  void OnDecodeOutput(scoped_refptr<media::VideoFrame> frame) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(&CrossThreadVideoDecoderClient::OnDecodeOutput,
                                 weak_client_, std::move(frame),
                                 decoder_->CanReadWithoutStalling()));
  }

  void OnDecodeDone(int cb_id, media::DecoderStatus status) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(&CrossThreadVideoDecoderClient::OnDecodeDone,
                                 weak_client_, cb_id, std::move(status)));
  }

  void OnReset(int cb_id) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(&CrossThreadVideoDecoderClient::OnReset,
                                 weak_client_, cb_id));
  }

  base::WeakPtr<CrossThreadVideoDecoderClient> weak_client_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  raw_ptr<media::GpuVideoAcceleratorFactories, DanglingUntriaged>
      gpu_factories_;
  std::unique_ptr<media::MediaLog> media_log_;
  mojo::Remote<media::mojom::InterfaceFactory> media_interface_factory_;
  std::unique_ptr<WebCodecsVideoDecoderSelector> selector_;
  std::unique_ptr<media::DecoderFactory> decoder_factory_;
  std::unique_ptr<media::VideoDecoder> decoder_;
  gfx::ColorSpace target_color_space_;
  HardwarePreference hardware_preference_ = HardwarePreference::kNoPreference;
  bool decoder_factory_needs_update_ = true;

#if BUILDFLAG(IS_FUCHSIA)
  mojo::PendingRemote<media::mojom::FuchsiaMediaCodecProvider>
      fuchsia_media_codec_provider_;
#endif

  SEQUENCE_CHECKER(sequence_checker_);

  // Using unretained for decoder/selector callbacks is generally not safe /
  // fragile. Some decoders (e.g. those that offload) will call the output
  // callback after destruction.
  base::WeakPtrFactory<MediaVideoTaskWrapper> weak_factory_{this};
};

VideoDecoderBroker::VideoDecoderBroker(
    ExecutionContext& execution_context,
    media::GpuVideoAcceleratorFactories* gpu_factories,
    media::MediaLog* media_log)
    : media_task_runner_(
          gpu_factories
              // GpuFactories requires we use its task runner when available.
              ? gpu_factories->GetTaskRunner()
              // Otherwise, use a worker task runner to avoid scheduling decoder
              // work on the main thread.
              : worker_pool::CreateSequencedTaskRunner({base::MayBlock()})) {
  DVLOG(2) << __func__;
  media_tasks_ = std::make_unique<MediaVideoTaskWrapper>(
      weak_factory_.GetWeakPtr(), execution_context, gpu_factories,
      media_log->Clone(), media_task_runner_,
      execution_context.GetTaskRunner(TaskType::kInternalMedia));
}

VideoDecoderBroker::~VideoDecoderBroker() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  media_task_runner_->DeleteSoon(FROM_HERE, std::move(media_tasks_));
}

media::VideoDecoderType VideoDecoderBroker::GetDecoderType() const {
  return decoder_details_ ? decoder_details_->decoder_id
                          : media::VideoDecoderType::kBroker;
}

bool VideoDecoderBroker::IsPlatformDecoder() const {
  return decoder_details_ ? decoder_details_->is_platform_decoder : false;
}

void VideoDecoderBroker::SetHardwarePreference(
    HardwarePreference hardware_preference) {
  PostCrossThreadTask(
      *media_task_runner_, FROM_HERE,
      WTF::CrossThreadBindOnce(&MediaVideoTaskWrapper::UpdateHardwarePreference,
                               WTF::CrossThreadUnretained(media_tasks_.get()),
                               hardware_preference));
}

void VideoDecoderBroker::Initialize(const media::VideoDecoderConfig& config,
                                    bool low_delay,
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
      WTF::CrossThreadBindOnce(&MediaVideoTaskWrapper::Initialize,
                               WTF::CrossThreadUnretained(media_tasks_.get()),
                               config, low_delay));
}

int VideoDecoderBroker::CreateCallbackId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // 0 and -1 are reserved by wtf::HashMap ("empty" and "deleted").
  while (++last_callback_id_ == 0 ||
         last_callback_id_ == std::numeric_limits<uint32_t>::max() ||
         pending_decode_cb_map_.Contains(last_callback_id_) ||
         pending_reset_cb_map_.Contains(last_callback_id_))
    ;

  return last_callback_id_;
}

void VideoDecoderBroker::OnInitialize(media::DecoderStatus status,
                                      std::optional<DecoderDetails> details) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(init_cb_);
  decoder_details_ = details;
  std::move(init_cb_).Run(status);
}

void VideoDecoderBroker::Decode(scoped_refptr<media::DecoderBuffer> buffer,
                                DecodeCB decode_cb) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int callback_id = CreateCallbackId();
  pending_decode_cb_map_.insert(callback_id, std::move(decode_cb));

  PostCrossThreadTask(
      *media_task_runner_, FROM_HERE,
      WTF::CrossThreadBindOnce(&MediaVideoTaskWrapper::Decode,
                               WTF::CrossThreadUnretained(media_tasks_.get()),
                               buffer, callback_id));
}

void VideoDecoderBroker::OnDecodeDone(int cb_id, media::DecoderStatus status) {
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

void VideoDecoderBroker::Reset(base::OnceClosure reset_cb) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int callback_id = CreateCallbackId();
  pending_reset_cb_map_.insert(callback_id, std::move(reset_cb));

  PostCrossThreadTask(
      *media_task_runner_, FROM_HERE,
      WTF::CrossThreadBindOnce(&MediaVideoTaskWrapper::Reset,
                               WTF::CrossThreadUnretained(media_tasks_.get()),
                               callback_id));
}

bool VideoDecoderBroker::NeedsBitstreamConversion() const {
  return decoder_details_ ? decoder_details_->needs_bitstream_conversion
                          : false;
}

bool VideoDecoderBroker::CanReadWithoutStalling() const {
  return can_read_without_stalling_;
}

int VideoDecoderBroker::GetMaxDecodeRequests() const {
  return decoder_details_ ? decoder_details_->max_decode_requests : 1;
}

void VideoDecoderBroker::OnReset(int cb_id) {
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

void VideoDecoderBroker::OnDecodeOutput(scoped_refptr<media::VideoFrame> frame,
                                        bool can_read_without_stalling) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(output_cb_);

  can_read_without_stalling_ = can_read_without_stalling;

  output_cb_.Run(std::move(frame));
}

}  // namespace blink
