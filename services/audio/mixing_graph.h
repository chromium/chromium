// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_MIXING_GRAPH_H_
#define SERVICES_AUDIO_MIXING_GRAPH_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_parameters.h"

namespace audio {

// The graph is mixing audio provided by multiple audio sources
// (AudioOutputStream::AudioSourceCallback instances) and represents the mix as
// a single AudioOutputStream::AudioSourceCallback, which in turn serves as an
// audio source providing audio buffers to AudioOutputStream for playback.
class MixingGraph : public media::AudioOutputStream::AudioSourceCallback {
 public:
  using InputCallback = media::AudioConverter::InputCallback;

  using OnMoreDataCallback =
      base::RepeatingCallback<void(const media::AudioBus&, base::TimeDelta)>;

  using OnErrorCallback = base::RepeatingCallback<void(
      media::AudioOutputStream::AudioSourceCallback::ErrorType)>;

  // A helper class for the clients to pass MixingGraph::Create() around as a
  // callback.
  using CreateCallback = base::OnceCallback<std::unique_ptr<MixingGraph>(
      const media::AudioParameters& output_params,
      OnMoreDataCallback on_more_data_cb,
      OnErrorCallback on_error_cb)>;

  // Represents an audio source as an input to MixingGraph.
  // An adapter from AudioOutputStream::AudioSourceCallback to
  // AudioConverter::InputCallback.
  class Input : public InputCallback {
   public:
    Input() = default;
    ~Input() override = default;
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

    // Returns the audio source parameters.
    virtual const media::AudioParameters& GetParams() const = 0;

    // Sets the volume.
    virtual void SetVolume(double volume) = 0;

    // Starts providing audio from |source_callback| to the graph which created
    // the given Input.
    virtual void Start(
        media::AudioOutputStream::AudioSourceCallback* source_callback) = 0;

    // Stops providing audio to the graph which created the given Input.
    virtual void Stop() = 0;
  };

  // Creates a graph which will provide the audio mix formatted as
  // |output_params| each time its
  // AudioOutputStream::AudioSourceCallback::OnMoreData() method is called, and
  // will also provide the audio mix to |on_more_data_cb| callback.
  // |on_error_cb| will be used to notify the client about audio rendering
  // errors.
  static std::unique_ptr<MixingGraph> Create(
      const media::AudioParameters& output_params,
      OnMoreDataCallback on_more_data_cb,
      OnErrorCallback on_error_cb);

  MixingGraph() = default;
  MixingGraph(const MixingGraph&) = delete;
  MixingGraph& operator=(const MixingGraph&) = delete;

  // Creates a graph input with given audio source parameters.
  virtual std::unique_ptr<Input> CreateInput(
      const media::AudioParameters& params) = 0;

 protected:
  friend class SyncMixingGraphInput;

  // Adds an input to the graph. To be called by Input::Start().
  virtual void AddInput(Input* node) = 0;
  // Removes an input from the graph. To be called by Input::Stop().
  virtual void RemoveInput(Input* node) = 0;
};

}  // namespace audio
#endif  // SERVICES_AUDIO_MIXING_GRAPH_H_
