// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_FORWARDING_AUDIO_STREAM_FACTORY_H_
#define CONTENT_BROWSER_MEDIA_FORWARDING_AUDIO_STREAM_FACTORY_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "content/browser/media/audio_muting_session.h"
#include "content/browser/media/audio_stream_broker.h"
#include "content/common/content_export.h"
#include "content/common/media/renderer_audio_input_stream_factory.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/mojom/audio_processing.mojom.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"

namespace service_manager {
class Connector;
}

namespace media {
class AudioParameters;
class UserInputMonitorBase;
}

namespace content {

class RenderFrameHost;
class WebContents;

// This class handles stream creation operations for a WebContents.
// This class is operated on the UI thread.
class CONTENT_EXPORT ForwardingAudioStreamFactory final
    : public WebContentsObserver {
 public:
  // Note: all methods of Core may only be called on the IO thread except for
  // the constructor and group_id(). The destruction of Core is posted to the
  // IO thread when the owning ForwardingAudioStreamFactory is destructed. For
  // using |core()|, two rules emerges.
  // 1) If a task is posted from the UI thread to the IO thread while the
  //    owning ForwardingAudioStreamFactory is alive, |core()| may be posted
  //    Unretained.
  // 2) If |core()| is held until the owning ForwardingAudioStreamFactory could
  //    potentially be destructed, or if a task is posted to the IO thread with
  //    the intention of accessing |core| after that task returns, using a raw
  //    pointer is not safe. In those cases, take a weak pointer using
  //    AsWeakPtr() and check it for validity before every use. The weak
  //    pointer may only be checked/dereferenced on the IO thread.
  class CONTENT_EXPORT Core final : public AudioStreamBroker::LoopbackSource {
   public:
    Core(base::WeakPtr<ForwardingAudioStreamFactory> owner,
         media::UserInputMonitorBase* user_input_monitor,
         std::unique_ptr<service_manager::Connector> connector,
         std::unique_ptr<AudioStreamBrokerFactory> factory);
    ~Core() final;

    const base::UnguessableToken& group_id() const { return group_id_; }

    // E.g. to override binder.
    service_manager::Connector* get_connector_for_testing() {
      return connector_.get();
    }

    base::WeakPtr<ForwardingAudioStreamFactory::Core> AsWeakPtr();

    // TODO(https://crbug.com/787806): Automatically restore streams on audio
    // service restart.
    void CreateInputStream(
        int render_process_id,
        int render_frame_id,
        const std::string& device_id,
        const media::AudioParameters& params,
        uint32_t shared_memory_count,
        bool enable_agc,
        audio::mojom::AudioProcessingConfigPtr processing_config,
        mojo::PendingRemote<mojom::RendererAudioInputStreamFactoryClient>
            renderer_factory_client);

    void AssociateInputAndOutputForAec(
        const base::UnguessableToken& input_stream_id,
        const std::string& raw_output_device_id);

    void CreateOutputStream(
        int render_process_id,
        int render_frame_id,
        const std::string& device_id,
        const media::AudioParameters& params,
        const base::Optional<base::UnguessableToken>& processing_id,
        mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient>
            client);

    void CreateLoopbackStream(
        int render_process_id,
        int render_frame_id,
        AudioStreamBroker::LoopbackSource* loopback_source,
        const media::AudioParameters& params,
        uint32_t shared_memory_count,
        bool mute_source,
        mojo::PendingRemote<mojom::RendererAudioInputStreamFactoryClient>
            renderer_factory_client);

    // Sets the muting state for all output streams created through this
    // factory.
    void SetMuted(bool muted);

    // AudioStreamLoopback::Source implementation
    void AddLoopbackSink(AudioStreamBroker::LoopbackSink* sink) final;
    void RemoveLoopbackSink(AudioStreamBroker::LoopbackSink* sink) final;
    const base::UnguessableToken& GetGroupID() final;  // Actually const.

   private:
    // For CleanupStreamsBelongingTo.
    friend class ForwardingAudioStreamFactory;

    using StreamBrokerSet = base::flat_set<std::unique_ptr<AudioStreamBroker>,
                                           base::UniquePtrComparator>;

    void CleanupStreamsBelongingTo(int render_process_id, int render_frame_id);

    void RemoveInput(AudioStreamBroker* handle);
    void RemoveOutput(AudioStreamBroker* handle);

    audio::mojom::StreamFactory* GetFactory();
    void ResetRemoteFactoryPtrIfIdle();
    void ResetRemoteFactoryPtr();

    media::UserInputMonitorBase* const user_input_monitor_;

    // Used for posting tasks the UI thread to communicate when a loopback
    // stream is started/stopped. Weak since |this| on the IO thread outlives
    // |owner| on the UI thread.
    const base::WeakPtr<ForwardingAudioStreamFactory> owner_;

    const std::unique_ptr<AudioStreamBrokerFactory> broker_factory_;

    // Unique id identifying all streams belonging to the WebContents owning
    // |this|.
    const base::UnguessableToken group_id_;

    const std::unique_ptr<service_manager::Connector> connector_;

    // Lazily acquired. Reset on connection error and when we no longer have any
    // streams. Note: we don't want muting to force the connection to be open,
    // since we want to clean up the service when not in use. If we have active
    // muting but nothing else, we should stop it and start it again when we
    // need to reacquire the factory for some other reason.
    mojo::Remote<audio::mojom::StreamFactory> remote_factory_;

    // Running id used for tracking audible streams. We keep count here to avoid
    // collisions.
    // TODO(https://crbug.com/830494): Refactor to make this unnecessary and
    // remove it.
    int stream_id_counter_ = 0;

    // Instantiated when |outputs_| should be muted, empty otherwise.
    base::Optional<AudioMutingSession> muter_;

    StreamBrokerSet inputs_;
    StreamBrokerSet outputs_;
    base::flat_set<AudioStreamBroker::LoopbackSink*> loopback_sinks_;

    base::WeakPtrFactory<ForwardingAudioStreamFactory::Core> weak_ptr_factory_{
        this};

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  // Returns the ForwardingAudioStreamFactory which takes care of stream
  // creation for |frame|. Returns null if |frame| is null or if the frame
  // doesn't belong to a WebContents.
  static ForwardingAudioStreamFactory* ForFrame(RenderFrameHost* frame);

  // Returns the ForwardingAudioStreamFactory::Core which takes care of stream
  // creation for |frame|. Returns null if |frame| is null or if the frame
  // doesn't belong to a WebContents.
  static ForwardingAudioStreamFactory::Core* CoreForFrame(
      RenderFrameHost* frame);

  // |web_contents| is null in the browser-privileged access case, i.e., when
  // the streams created with this factory will not be consumed by a renderer.
  // |connector| will be used on the IO thread.
  ForwardingAudioStreamFactory(
      WebContents* web_contents,
      media::UserInputMonitorBase* user_input_monitor,
      std::unique_ptr<service_manager::Connector> connector,
      std::unique_ptr<AudioStreamBrokerFactory> factory);

  ~ForwardingAudioStreamFactory() final;

  const base::UnguessableToken& group_id() const {
    // Thread-safe since Core::group_id is thread-safe and |core_| outlives
    // |this|.
    return core_->group_id();
  }

  void LoopbackStreamStarted();
  void LoopbackStreamStopped();

  // Sets the muting state for all output streams created through this factory.
  void SetMuted(bool muted);

  // Returns the current muting state.
  bool IsMuted() const;

  // WebContentsObserver implementation. We observe these events so that we can
  // clean up streams belonging to a frame when that frame is destroyed.
  void FrameDeleted(RenderFrameHost* render_frame_host) final;

  Core* core() { return core_.get(); }

 private:
  std::unique_ptr<Core> core_;
  bool is_muted_ = false;

  base::WeakPtrFactory<ForwardingAudioStreamFactory> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ForwardingAudioStreamFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_FORWARDING_AUDIO_STREAM_FACTORY_H_
