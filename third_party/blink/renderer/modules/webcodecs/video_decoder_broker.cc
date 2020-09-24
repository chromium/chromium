// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_decoder_broker.h"

#include <limits>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "build/buildflag.h"
#include "media/base/decoder_factory.h"
#include "media/base/media_util.h"
#include "media/base/status_codes.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/clients/mojo_decoder_factory.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/renderers/default_decoder_factory.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
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
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/color_space.h"

using DecoderDetails = blink::VideoDecoderBroker::DecoderDetails;

namespace WTF {

template <>
struct CrossThreadCopier<media::VideoDecoderConfig>
    : public CrossThreadCopierPassThrough<media::VideoDecoderConfig> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<media::Status>
    : public CrossThreadCopierPassThrough<media::Status> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<base::Optional<DecoderDetails>>
    : public CrossThreadCopierPassThrough<base::Optional<DecoderDetails>> {
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
      WTF::CrossThreadOnceFunction<void(media::Status status,
                                        base::Optional<DecoderDetails>)>;
  using CrossThreadOnceDecodeCB =
      WTF::CrossThreadOnceFunction<void(const media::Status&)>;
  using CrossThreadOnceResetCB = WTF::CrossThreadOnceClosure;

  MediaVideoTaskWrapper(
      base::WeakPtr<CrossThreadVideoDecoderClient> weak_client,
      ExecutionContext& execution_context,
      media::GpuVideoAcceleratorFactories* gpu_factories,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
      : weak_client_(std::move(weak_client)),
        media_task_runner_(std::move(media_task_runner)),
        main_task_runner_(std::move(main_task_runner)),
        gpu_factories_(gpu_factories) {
    DVLOG(2) << __func__;
    DETACH_FROM_SEQUENCE(sequence_checker_);

    // TODO(chcunningham): Enable this for workers. Currently only a
    // frame-binding (RenderFrameHostImpl) is exposed.
    // TODO(chcunningham): set_disconnect_handler?
    // Mojo connection setup must occur here on the main thread where its safe
    // to use |execution_context| APIs.
    mojo::PendingRemote<media::mojom::InterfaceFactory> media_interface_factory;
    execution_context.GetBrowserInterfaceBroker().GetInterface(
        media_interface_factory.InitWithNewPipeAndPassReceiver());

    // Mojo remote must be bound on media thread where it will be used.
    //|Unretained| is safe because |this| must be destroyed on the media task
    // runner.
    PostCrossThreadTask(
        *media_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(&MediaVideoTaskWrapper::BindOnTaskRunner,
                                 WTF::CrossThreadUnretained(this),
                                 std::move(media_interface_factory)));

    // TODO(chcunningham): Research usage of this and consider how to unify for
    // worker context (no document). What follows is borrowed from
    // HTMLMediaElement.
    Document* document = To<LocalDOMWindow>(execution_context).document();
    if (document && document->GetFrame()) {
      LocalFrame* frame = document->GetFrame();
      target_color_space_ = frame->GetPage()
                                ->GetChromeClient()
                                .GetScreenInfo(*frame)
                                .display_color_spaces.GetScreenInfoColorSpace();
    }
  }

  virtual ~MediaVideoTaskWrapper() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  MediaVideoTaskWrapper(const MediaVideoTaskWrapper&) = delete;
  MediaVideoTaskWrapper& operator=(const MediaVideoTaskWrapper&) = delete;

  void Initialize(const media::VideoDecoderConfig& config) {
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
        config, WTF::Bind(&MediaVideoTaskWrapper::OnDecoderSelected,
                          weak_factory_.GetWeakPtr()));
  }

  void Decode(scoped_refptr<media::DecoderBuffer> buffer, int cb_id) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!decoder_) {
      OnDecodeDone(cb_id, media::DecodeStatus::DECODE_ERROR);
      return;
    }

    decoder_->Decode(std::move(buffer),
                     WTF::Bind(&MediaVideoTaskWrapper::OnDecodeDone,
                               weak_factory_.GetWeakPtr(), cb_id));
  }

  void Reset(int cb_id) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!decoder_) {
      OnReset(cb_id);
      return;
    }

    decoder_->Reset(WTF::Bind(&MediaVideoTaskWrapper::OnReset,
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
#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
    external_decoder_factory = std::make_unique<media::MojoDecoderFactory>(
        media_interface_factory_.get());
#endif
    decoder_factory_ = std::make_unique<media::DefaultDecoderFactory>(
        std::move(external_decoder_factory));
  }

  std::vector<std::unique_ptr<media::VideoDecoder>> OnCreateDecoders() {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // TODO(chcunningham): Add plumbing to enable overlays on Android. See
    // handling in WebMediaPlayerImpl.
    media::RequestOverlayInfoCB request_overlay_info_cb;

    std::vector<std::unique_ptr<media::VideoDecoder>> video_decoders;
    decoder_factory_->CreateVideoDecoders(
        media_task_runner_, gpu_factories_, &null_media_log_,
        request_overlay_info_cb, target_color_space_, &video_decoders);

    return video_decoders;
  }

  void OnDecoderSelected(std::unique_ptr<media::VideoDecoder> decoder) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // We're done with it.
    DCHECK(selector_);
    selector_.reset();

    decoder_ = std::move(decoder);

    media::Status status(media::StatusCode::kDecoderUnsupportedConfig);
    base::Optional<DecoderDetails> decoder_details;
    if (decoder_) {
      status = media::OkStatus();
      decoder_details = DecoderDetails({decoder_->GetDisplayName(),
                                        decoder_->IsPlatformDecoder(),
                                        decoder_->NeedsBitstreamConversion(),
                                        decoder_->GetMaxDecodeRequests()});
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

  void OnDecodeDone(int cb_id, media::Status status) {
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
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  media::GpuVideoAcceleratorFactories* gpu_factories_;
  mojo::Remote<media::mojom::InterfaceFactory> media_interface_factory_;
  std::unique_ptr<WebCodecsVideoDecoderSelector> selector_;
  std::unique_ptr<media::DefaultDecoderFactory> decoder_factory_;
  std::unique_ptr<media::VideoDecoder> decoder_;
  gfx::ColorSpace target_color_space_;

  // TODO(chcunningham): Route MEDIA_LOG for WebCodecs.
  media::NullMediaLog null_media_log_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Using unretained for decoder/selector callbacks is generally not safe /
  // fragile. Some decoders (e.g. those that offload) will call the output
  // callback after destruction.
  base::WeakPtrFactory<MediaVideoTaskWrapper> weak_factory_{this};
};

constexpr char VideoDecoderBroker::kDefaultDisplayName[];

VideoDecoderBroker::VideoDecoderBroker(
    ExecutionContext& execution_context,
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : media_task_runner_(
          gpu_factories
              ? gpu_factories->GetTaskRunner()
              // TODO(chcunningham): Consider adding a new single thread task
              // runner just for WebCodecs. This is still using the main thread,
              // albeit at a lower priority than things like user gestures.
              // http://crbug.com/1095786
              // TODO(chcunningham): Should this be kInternalMediaRealTime? Why
              // does WebAudio use that task type?
              : execution_context.GetTaskRunner(TaskType::kInternalMedia)) {
  DVLOG(2) << __func__;
  media_tasks_ = std::make_unique<MediaVideoTaskWrapper>(
      weak_factory_.GetWeakPtr(), execution_context, gpu_factories,
      media_task_runner_,
      execution_context.GetTaskRunner(TaskType::kInternalMedia));
}

VideoDecoderBroker::~VideoDecoderBroker() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  media_task_runner_->DeleteSoon(FROM_HERE, std::move(media_tasks_));
}

std::string VideoDecoderBroker::GetDisplayName() const {
  return decoder_details_ ? decoder_details_->display_name
                          : VideoDecoderBroker::kDefaultDisplayName;
}

bool VideoDecoderBroker::IsPlatformDecoder() const {
  return decoder_details_ ? decoder_details_->is_platform_decoder : false;
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
  // TODO(chcunningham): Should |low_delay| be supported? Should it be
  // hard-coded to true?
  DCHECK(!low_delay);
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
                               config));
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

void VideoDecoderBroker::OnInitialize(media::Status status,
                                      base::Optional<DecoderDetails> details) {
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

void VideoDecoderBroker::OnDecodeDone(int cb_id, media::Status status) {
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
