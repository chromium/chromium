// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DATA_SOURCE_H_
#define MEDIA_BASE_DATA_SOURCE_H_

#include <stdint.h>

#include "base/functional/callback_forward.h"
#include "media/base/media_export.h"
#include "url/gurl.h"

namespace media {

class CrossOriginDataSource;

// Abstracting informational methods into DataSourceInfo allows
// "meta-datasource" objects like HlsDataSourceProvider to query it's entire
// set of dependent data sources when calculating the data here.
class MEDIA_EXPORT DataSourceInfo {
 public:
  virtual ~DataSourceInfo();

  // The total memory size used.
  virtual int64_t GetMemoryUsage() = 0;

  // DataSource implementations that might make requests must ensure the value
  // is accurate for cross origin resources.
  virtual bool WouldTaintOrigin() = 0;

  // Returns true if we are performing streaming. In this case seeking is
  // not possible.
  virtual bool IsStreaming() = 0;
};

class MEDIA_EXPORT DataSource : public DataSourceInfo {
 public:
  using ReadCB = base::OnceCallback<void(int)>;

  enum { kReadError = -1, kAborted = -2 };

  // Used to specify video preload states. They are "hints" to the browser about
  // how aggressively the browser should load and buffer data.
  // Please see the HTML5 spec for the descriptions of these values:
  // http://www.w3.org/TR/html5/video.html#attr-media-preload
  //
  // Enum values must match the values in WebMediaPlayer::Preload and
  // there will be assertions at compile time if they do not match.
  enum Preload {
    NONE,
    METADATA,
    AUTO,
  };

  DataSource();

  DataSource(const DataSource&) = delete;
  DataSource& operator=(const DataSource&) = delete;

  ~DataSource() override;

  // Reads |size| bytes from |position| into |data|. And when the read is done
  // or failed, |read_cb| is called with the number of bytes read or
  // kReadError in case of error.
  virtual void Read(int64_t position,
                    int size,
                    uint8_t* data,
                    DataSource::ReadCB read_cb) = 0;

  // Stops the DataSource. Once this is called all future Read() calls will
  // return an error. This is a synchronous call and may be called from any
  // thread. Once called, the DataSource may no longer be used and should be
  // destructed shortly thereafter.
  virtual void Stop() = 0;

  // Similar to Stop(), but only aborts current reads and not future reads.
  virtual void Abort() = 0;

  // Returns true and the file size, false if the file size could not be
  // retrieved.
  [[nodiscard]] virtual bool GetSize(int64_t* size_out) = 0;

  // Notify the DataSource of the bitrate of the media.
  // Values of |bitrate| <= 0 are invalid and should be ignored.
  virtual void SetBitrate(int bitrate) = 0;

  // If there is a MultiBuffer associated with this data source, then defer to
  // it. This will return false if any HTTP response so far has failed the TAO
  // check.
  virtual bool PassedTimingAllowOriginCheck() = 0;

  // Assume fully buffered by default.
  virtual bool AssumeFullyBuffered() const;

  // By default this just returns GetSize().
  int64_t GetMemoryUsage() override;

  // Adjusts the buffering algorithm (if there is one) based on the given
  // preload value.
  virtual void SetPreload(media::DataSource::Preload preload);

  // Gets the url for this data source, if it exists. By default this returns
  // an empty GURL.
  virtual GURL GetUrlAfterRedirects() const;

  // Cancels any open network connections once reaching the deferred state. If
  // |must_cancel_netops| is false this is done only for preload=metadata, non-
  // streaming resources that have not started playback. If |must_cancel_netops|
  // is true, all resource types will have their connections canceled. If
  // already deferred, connections will be immediately closed. Most data source
  // implementations do not need to override this, as they do not open other
  // network connections.
  virtual void OnBufferingHaveEnough(bool must_cancel_netops);

  // Allows a holder to notify certain events to the data source. Most data
  // sources won't care too much about these events though.
  virtual void OnMediaPlaybackRateChanged(double playback_rate);
  virtual void OnMediaIsPlaying();

  // Gets a CrossOriginDataSource version of |this|, or nullptr if it isn't one.
  virtual CrossOriginDataSource* GetAsCrossOriginDataSource();
};

}  // namespace media

#endif  // MEDIA_BASE_DATA_SOURCE_H_
