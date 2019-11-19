// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_TEXT_RENDERER_H_
#define MEDIA_BASE_TEXT_RENDERER_H_

#include <map>
#include <memory>
#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "media/base/text_ranges.h"
#include "media/base/text_track.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class TextCue;
class TextTrackConfig;

// Receives decoder buffers from the upstream demuxer, decodes them to text
// cues, and then passes them onto the TextTrack object associated with each
// demuxer text stream.
class MEDIA_EXPORT TextRenderer {
 public:
  // |task_runner| is the thread on which TextRenderer will execute.
  //
  // |add_text_track_cb] is called when the demuxer requests (via its host)
  // that a new text track be created.
  TextRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const AddTextTrackCB& add_text_track_cb);

  // Stops all operations and fires all pending callbacks.
  ~TextRenderer();

  // |ended_cb| is executed when all of the text tracks have reached
  // end of stream, following a play request.
  void Initialize(const base::Closure& ended_cb);

  // Starts text track cue decoding and rendering.
  void StartPlaying();

  // Temporarily suspends decoding and rendering, executing |callback| when
  // playback has been suspended.
  void Pause(const base::Closure& callback);

  // Discards any text data, executing |callback| when completed.
  void Flush(const base::Closure& callback);

  // Adds new |text_stream|, having the indicated |config|, to the text stream
  // collection managed by this text renderer.
  void AddTextStream(DemuxerStream* text_stream,
                     const TextTrackConfig& config);

  // Removes |text_stream| from the text stream collection.
  void RemoveTextStream(DemuxerStream* text_stream);

  // Returns true if there are extant text tracks.
  bool HasTracks() const;

 private:
  struct TextTrackState {
    // To determine read progress.
    enum ReadState {
      kReadIdle,
      kReadPending
    };

    explicit TextTrackState(std::unique_ptr<TextTrack> text_track);
    ~TextTrackState();

    ReadState read_state;
    std::unique_ptr<TextTrack> text_track;
    TextRanges text_ranges_;
  };

  // Callback delivered by the demuxer |text_stream| when
  // a read from the stream completes.
  void BufferReady(DemuxerStream* text_stream,
                   DemuxerStream::Status status,
                   scoped_refptr<DecoderBuffer> input);

  // Dispatches the decoded cue delivered on the demuxer's |text_stream|.
  void CueReady(DemuxerStream* text_stream,
                const scoped_refptr<TextCue>& text_cue);

  // Dispatched when the AddTextTrackCB completes, after having created
  // the TextTrack object associated with |text_stream|.
  void OnAddTextTrackDone(DemuxerStream* text_stream,
                          std::unique_ptr<TextTrack> text_track);

  // Utility function to post a read request on |text_stream|.
  void Read(TextTrackState* state, DemuxerStream* text_stream);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const AddTextTrackCB add_text_track_cb_;

  // Callbacks provided during Initialize().
  base::Closure ended_cb_;

  // Callback provided to Pause().
  base::Closure pause_cb_;

  // Simple state tracking variable.
  enum State {
    kUninitialized,
    kPausePending,
    kPaused,
    kPlaying,
    kEnded
  };
  State state_;

  std::map<DemuxerStream*, std::unique_ptr<TextTrackState>>
      text_track_state_map_;

  // Indicates how many read requests are in flight.
  int pending_read_count_;

  // Indicates which text streams have not delivered end-of-stream yet.
  typedef std::set<DemuxerStream*> PendingEosSet;
  PendingEosSet pending_eos_set_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<TextRenderer> weak_factory_{this};

  DISALLOW_IMPLICIT_CONSTRUCTORS(TextRenderer);
};

}  // namespace media

#endif  // MEDIA_BASE_TEXT_RENDERER_H_
