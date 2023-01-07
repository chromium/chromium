// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_TRACKS_BUILDER_H_
#define MEDIA_FORMATS_WEBM_TRACKS_BUILDER_H_

#include <stdint.h>

#include <list>
#include <string>
#include <vector>

namespace media {

class TracksBuilder {
 public:
  // If |allow_invalid_values| is false, some AddTrack() parameters will be
  // basically checked and will assert if out of valid range. |codec_id|,
  // |name|, |language| and any device-specific constraints are not checked.
  explicit TracksBuilder(bool allow_invalid_values);
  TracksBuilder();  // Sets |allow_invalid_values| to false.

  TracksBuilder(const TracksBuilder&) = delete;
  TracksBuilder& operator=(const TracksBuilder&) = delete;

  ~TracksBuilder();

  // Only a non-negative |default_duration| will result in a serialized
  // kWebMIdDefaultDuration element. Note, 0 is allowed here for testing only
  // if |allow_invalid_values_| is true, since it is an illegal value for
  // DefaultDuration. Similar applies to |audio_channels|,
  // |audio_sampling_frequency|, |video_pixel_width| and |video_pixel_height|.
  void AddVideoTrack(int track_num,
                     uint64_t track_uid,
                     const std::string& codec_id,
                     const std::string& name,
                     const std::string& language,
                     int default_duration,
                     int video_pixel_width,
                     int video_pixel_height);
  void AddAudioTrack(int track_num,
                     uint64_t track_uid,
                     const std::string& codec_id,
                     const std::string& name,
                     const std::string& language,
                     int default_duration,
                     int audio_channels,
                     double audio_sampling_frequency);
  void AddTextTrack(int track_num,
                    uint64_t track_uid,
                    const std::string& codec_id,
                    const std::string& name,
                    const std::string& language);

  std::vector<uint8_t> Finish();

 private:
  void AddTrackInternal(int track_num,
                        int track_type,
                        uint64_t track_uid,
                        const std::string& codec_id,
                        const std::string& name,
                        const std::string& language,
                        int default_duration,
                        int video_pixel_width,
                        int video_pixel_height,
                        int audio_channels,
                        double audio_sampling_frequency);
  int GetTracksSize() const;
  int GetTracksPayloadSize() const;
  void WriteTracks(uint8_t* buffer, int buffer_size) const;

  class Track {
   public:
    Track(int track_num,
          int track_type,
          uint64_t track_uid,
          const std::string& codec_id,
          const std::string& name,
          const std::string& language,
          int default_duration,
          int video_pixel_width,
          int video_pixel_height,
          int audio_channels,
          double audio_sampling_frequency,
          bool allow_invalid_values);
    Track(const Track& other);

    int GetSize() const;
    void Write(uint8_t** buf, int* buf_size) const;

   private:
    int GetPayloadSize() const;
    int GetVideoPayloadSize() const;
    int GetAudioPayloadSize() const;

    int track_num_;
    int track_type_;
    int track_uid_;
    std::string codec_id_;
    std::string name_;
    std::string language_;
    int default_duration_;
    int video_pixel_width_;
    int video_pixel_height_;
    int audio_channels_;
    double audio_sampling_frequency_;
  };

  typedef std::list<Track> TrackList;
  TrackList tracks_;
  bool allow_invalid_values_;
};

}  // namespace media

#endif  // MEDIA_FORMATS_WEBM_TRACKS_BUILDER_H_
