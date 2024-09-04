// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_MEDIA_SEGMENT_H_
#define MEDIA_FORMATS_HLS_MEDIA_SEGMENT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "crypto/symmetric_key.h"
#include "media/base/media_export.h"
#include "media/formats/hls/tags.h"
#include "media/formats/hls/types.h"
#include "url/gurl.h"

namespace media::hls {

class MEDIA_EXPORT MediaSegment : public base::RefCounted<MediaSegment> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  class MEDIA_EXPORT InitializationSegment
      : public base::RefCounted<InitializationSegment> {
   public:
    REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

    InitializationSegment(GURL uri, std::optional<types::ByteRange>);
    InitializationSegment(const InitializationSegment& copy) = delete;
    InitializationSegment(InitializationSegment&& copy) = delete;
    InitializationSegment& operator=(const InitializationSegment& copy) =
        delete;
    InitializationSegment& operator=(InitializationSegment&& copy) = delete;

    // The URI, resolved against the playlist URI, of the resource containing
    // the initialization segment.
    const GURL& GetUri() const { return uri_; }

    // If the initialization segment is a subrange of its resource, this
    // indicates the range.
    std::optional<types::ByteRange> GetByteRange() const { return byte_range_; }

   private:
    friend class base::RefCounted<InitializationSegment>;
    ~InitializationSegment();

    GURL uri_;
    std::optional<types::ByteRange> byte_range_;
  };

  class MEDIA_EXPORT EncryptionData : public base::RefCounted<EncryptionData> {
   public:
    REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

    using IVType = types::parsing::HexRepr<128>;
    using IVContainer = std::optional<IVType::Container>;

    EncryptionData(GURL uri,
                   XKeyTagMethod method,
                   XKeyTagKeyFormat format,
                   IVContainer iv);
    EncryptionData(const EncryptionData& copy) = delete;
    EncryptionData(EncryptionData&& copy) = delete;
    EncryptionData& operator=(const EncryptionData& copy) = delete;
    EncryptionData& operator=(EncryptionData&& copy) = delete;

    const GURL& GetUri() const { return uri_; }
    XKeyTagMethod GetMethod() const { return method_; }
    crypto::SymmetricKey* GetKey() const { return key_.get(); }
    XKeyTagKeyFormat GetKeyFormat() const { return format_; }

    bool NeedsKeyFetch() const { return !key_; }

    // Gets the InitializationVector, if it exists. If there is no IV, but the
    // `identity_` flag is set, then use the media sequence number as the IV.
    IVContainer GetIV(types::DecimalInteger media_sequence_number) const;

    // Pack the IV into a string for use in a crypto::Encryptor.
    std::optional<std::string> GetIVStr(
        types::DecimalInteger media_sequence_number) const;

    // When `uri_` is fetched, import the raw data.
    void ImportKey(std::string_view key_content);

   private:
    friend class base::RefCounted<EncryptionData>;
    ~EncryptionData();

    const GURL uri_;
    const XKeyTagMethod method_;
    const IVContainer iv_;
    const XKeyTagKeyFormat format_;

    // Used for clear key AES128 and AES256 full segment encryption.
    std::unique_ptr<crypto::SymmetricKey> key_;
  };

  MediaSegment(base::TimeDelta duration,
               types::DecimalInteger media_sequence_number,
               types::DecimalInteger discontinuity_sequence_number,
               GURL uri,
               scoped_refptr<InitializationSegment> initialization_segment,
               scoped_refptr<EncryptionData> encryption_data,
               std::optional<types::ByteRange> byte_range,
               std::optional<types::DecimalInteger> bitrate,
               bool has_discontinuity,
               bool is_gap,
               bool has_new_init_segment,
               bool has_new_encryption_data);
  MediaSegment(const MediaSegment&) = delete;
  MediaSegment(MediaSegment&&) = delete;
  MediaSegment& operator=(const MediaSegment&) = delete;
  MediaSegment& operator=(MediaSegment&&) = delete;

  // The approximate duration of this media segment.
  base::TimeDelta GetDuration() const { return duration_; }

  // Returns the media sequence number of this media segment.
  types::DecimalInteger GetMediaSequenceNumber() const {
    return media_sequence_number_;
  }

  // Returns the discontinuity sequence number of this media segment.
  types::DecimalInteger GetDiscontinuitySequenceNumber() const {
    return discontinuity_sequence_number_;
  }

  // The URI of the media resource. This will have already been resolved against
  // the playlist URI. This is guaranteed to be valid and non-empty, unless
  // `gap` is true, in which case this URI should not be used.
  const GURL& GetUri() const { return uri_; }

  // Returns the initialization segment for this media segment, which may be
  // null if this segment has none. Subsequent media segments may also share the
  // same initialization segment.
  scoped_refptr<InitializationSegment> GetInitializationSegment() const {
    return initialization_segment_;
  }

  // Returns whether this MediaSegment has a different InitializationSegment
  // than the MediaSegment which immediately preceded this.
  bool HasNewInitSegment() const { return has_new_init_segment_; }

  // Returns the encryption data for this media segment, which may be null if
  // this segment has none. Subsequent media segments may also share the same
  // encryption data.
  scoped_refptr<EncryptionData> GetEncryptionData() const {
    return encryption_data_;
  }

  // Returns whether this MediaSegment has a different EncryptionData from the
  // MediaSegment which immediately preceded it.
  bool HasNewEncryptionData() const { return has_new_encryption_data_; }

  // If this media segment is a subrange of its resource, this indicates the
  // range.
  std::optional<types::ByteRange> GetByteRange() const { return byte_range_; }

  // Whether there is a decoding discontinuity between the previous media
  // segment and this one.
  bool HasDiscontinuity() const { return has_discontinuity_; }

  // If this is `true`, it indicates that the resource for this media segment is
  // absent and the client should not attempt to fetch it.
  bool IsGap() const { return is_gap_; }

  // Returns the approximate bitrate of this segment (+-10%), expressed in
  // bits-per-second.
  std::optional<types::DecimalInteger> GetBitRate() const { return bitrate_; }

 private:
  friend class base::RefCounted<MediaSegment>;
  ~MediaSegment();

  base::TimeDelta duration_;
  types::DecimalInteger media_sequence_number_;
  types::DecimalInteger discontinuity_sequence_number_;
  GURL uri_;
  scoped_refptr<InitializationSegment> initialization_segment_;

  scoped_refptr<EncryptionData> encryption_data_;
  std::optional<types::ByteRange> byte_range_;
  std::optional<types::DecimalInteger> bitrate_;
  bool has_discontinuity_;
  bool is_gap_;
  bool has_new_init_segment_;
  bool has_new_encryption_data_;
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_MEDIA_SEGMENT_H_
