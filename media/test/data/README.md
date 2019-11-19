# Media Test Data

[TOC]

## Instructions

To add, update or remove a test file, please update the list below.

Please provide full reference and steps to generate the test file so that
any people can regenerate or update the file in the future.

## List of Test Files

### General Test Files

#### bear-320x240.webm
WebM encode of bear.1280x720.mp4 resized to 320x240.

#### bear-320x240-video-only.webm
The video track of bear-320x240.webm.

#### bear-320x240-audio-only.webm
The audio track of bear-320x240.webm.

#### bear-vp9.webm
VP9 video only WebM file.

#### bear-vp9-opus.webm
VP9 Video with Opus Audio.

#### bear-opus.webm
Opus Audio only WebM file.

#### bear-vp8-webvtt.webm
WebM VP8 video with WebVTT subtitle track.

#### bear-1280x720.mp4
AAC audio and H264 high profile video.

#### bear-1280x720-avt_subt_frag.mp4
Fragmented bear_1280x720.mp4 with text track containing srt from
bear-vp8-webvtt.webm as a 'subt' handler type.

#### bear-1280x720-av_frag.mp4 - Fragmented bear_1280x720.mp4.
#### bear-1280x720-av_frag-initsegment-mvhd_version_0-mvhd_duration_bits_all_set.mp4:
Just the first initialization segment of bear-1280x720_av_frag.mp4, modified to
have the mvhd version 0 32-bit duration field set to all 1's.

#### media/test/data/negative-audio-timestamps.avi
A truncated audio/video file with audio packet timestamps of -1. We need to ensure that these packets arent dropped.

### FLAC

#### bear-flac.mp4
Unfragmented audio-only 44.1kHz FLAC in MP4 file, created using:
```
ffmpeg -i bear-1280x720.mp4 -map 0:0 -acodec flac -strict -2 bear-flac.mp4
```
**Note**: "-strict -2" was required because current ffmpeg libavformat version
57.75.100 indicates that flac in MP4 support is experimental.

#### bear-flac_frag.mp4
Fragmented audio-only 44.1kHz FLAC in MP4 file, created using:
```
ffmpeg -i bear-flac.mp4 -acodec copy -strict -2 -movflags frag_keyframe+empty_moov+default_base_moof bear-flac_frag.mp4
```

#### bear-flac-192kHz.mp4
Unfragmented audio-only high-sample-rate FLAC in MP4 file, created using:
```
ffmpeg -i bear-1280x720.mp4 -map 0:0 -acodec flac -strict -2 -ar 192000 bear-flac-192kHz.mp4
```

#### bear-flac-192kHz_frag.mp4
Fragmented audio-only high-sample-rate FLAC in MP4 file, created using:
```
ffmpeg -i bear-flac-192kHz.mp4 -acodec copy -strict -2 -movflags frag_keyframe+empty_moov+default_base_moof bear-flac-192kHz_frag.mp4
```

#### sfx-flac.mp4
Unfragmented audio-only 44.1kHz FLAC in MP4 file, created using:
```
ffmpeg -i sfx.flac -map 0:0 -acodec copy -strict -2 sfx-flac.mp4
```

#### sfx-flac_frag.mp4
Fragmented audio-only 44.1kHz FLAC in MP4 file, created using:
```
ffmpeg -i sfx.flac -map 0:0 -acodec copy -strict -2 -movflags frag_keyframe+empty_moov+default_base_moof sfx-flac_frag.mp4
```

### AV1

Unless noted otherwise, the codec string is `av01.0.04M.08` for 8-bit files,
and `av01.0.04M.10` for 10-bit files.

#### bear.y4m
Not an AV1 file, but all of the following commands rely on this file. It was
created using vpxdec with the following command:
```
vpxdec path/to/chrome/src/media/test/data/bear-vp9.webm -o bear.y4m
```

#### bear-av1.mp4
Created using FFmpeg with the following commands:
```
ffmpeg -i bear.y4m -vcodec libaom-av1 -strict -2 -y -f mp4 -b:v 50k \
  bear-av1-slowstart.mp4
ffmpeg -i bear-av1-slowstart.mp4 -vcodec copy -strict -2 -y -f mp4 \
  -movflags frag_keyframe+empty_moov+default_base_moof+faststart bear-av1.mp4
```

#### bear-av1.webm
Created using aomenc with the following command:
```
aomenc bear.y4m --lag-in-frames=0 --target-bitrate=50 --fps=30000/1001 \
  --cpu-used=8 --test-decode=fatal -o bear-av1.webm
```

#### bear-av1-480x360.webm
Created using FFmpeg and aomenc with the following commands:
```
ffmpeg -i bear.y4m -vf scale=-1:360 -f rawvideo bear_360P.yuv
aomenc bear_360P.yuv -w 480 -h 360 --fps=30000/1001 --cpu-used=8 \
  --lag-in-frames=0 --test-decode=fatal --target-bitrate=50 \
  -o bear-av1-480x360.webm
```

#### bear-av1-640x480.webm
Created using FFmpeg and aomenc with the following commands:
```
ffmpeg -i bear.y4m -vf scale=-1:480 -f rawvideo bear_480P.yuv
aomenc bear_480P.yuv -w 640 -h 480 --fps=30000/1001 --cpu-used=8 \
  --lag-in-frames=0 --test-decode=fatal --target-bitrate=50 \
  -o bear-av1-640x480.webm
```

#### bear-av1-320x180-10bit.mp4
Created using FFmpeg with the following command:
```
ffmpeg -i bear-av1-320x180-10bit.webm -vcodec copy -f mp4 \
  -movflags frag_keyframe+empty_moov+default_base_moof+faststart \
  bear-av1-320x180-10bit.mp4
```

#### bear-av1-320x180-10bit.webm
Created using vpxdec and aomenc with the following commands:
```
vpxdec bear-320x180-hi10p-vp9.webm -o bear-320x180-10bit.y4m
aomenc bear-320x180-10bit.y4m --lag-in-frames=0 --target-bitrate=50 \
  --fps=30000/1001 --cpu-used=8 --bit-depth=10 --test-decode=fatal \
  -o bear-av1-320x180-10bit.webm
```

#### bear-av1-opus.mp4
Created by combining bear-av1.mp4 and bear-opus.mp4.
```
ffmpeg -i bear-av1.mp4 -i bear-opus.mp4 -c copy -strict -2 \
  -movflags frag_keyframe+empty_moov+default_base_moof \
  bear-av1-opus.mp4
```
**Note**: "-strict -2" was required because the current ffmpeg version
has support for OPUS in MP4 as experimental.

#### av1-svc-L2T2.ivf
AV1 data that has spatial and temporal layers.
This is the same as av1-1-b8-22-svc-L2T2.ivf in
[libaom test vectors]:https://aomedia.googlesource.com/aom/+/master/test/test_vectors.cc

#### av1-show_existing_frame.ivf
AV1 data that contains frames with `show_existing_frame=1`.
This is the same as 00000592.ivf in
https://people.xiph.org/~tterribe/av1/samples-all/


### Alpha Channel

#### bear-vp8a.webm
WebM VP8 video with alpha channel.

#### bear-vp8a-odd-dimensions.webm
WebM VP8 video with alpha channel and odd dimensions.

### VP8 Frame Data

#### vp8-I-frame-160x240
The first I frame of a 160x240 re-encode of bear-320x240.webm.

#### vp8-I-frame-320x120
The first I frame of a 320x120 re-encode of bear-320x240.webm.

#### vp8-I-frame-320x240
The first I frame of bear-320x240.webm.

#### vp8-P-frame-320x240
The second P frame of bear-320x240.webm.

#### vp8-I-frame-320x480
The first I frame of a 320x480 re-encode of bear-320x240.webm.

#### vp8-I-frame-640x240
The first I frame of a 640x240 re-encode of bear-320x240.webm.

#### vp8-corrupt-I-frame
A copy of vp8-I-frame-320x240 w/ all bytes XORed w/ 0xA5.

### Corrupted Files

#### no_streams.webm
Header, Info, & Tracks element from bear-320x240.webm slightly corrupted so it
looks like there are no tracks.

#### nonzero-start-time.webm
Has the same headers as bear-320x240.webm but the first cluster of this file
is the second cluster of bear-320x240.webm. This creates the situation where
the media data doesn't start at time 0.

#### bear-320x240_corrupted_after_init_segment.webm
bear-320x240.webm's initialization segment followed by "CORRUPTED\n"

### Live

#### bear-320x240-live.webm
bear-320x240.webm remuxed w/o a duration and using clusters with unknown sizes.
```
ffmpeg -i bear-320x240.webm -acodec copy -vcodec copy -f webm pipe:1 > bear-320x240-live.webm
```

### Color / High Bit Depth

#### colour.webm
a WebM file containing color metadata in MKV/WebM Colour element copied from
libwebm/testing/testdata/colour.webm

#### four-colors.mp4
A 960x540 H.264 mp4 video with 4 color blocks (Y,R,G,B) in every frame. The
video playback looks like a still image. An image of 4 color blocks (.png file)
is first created by Windows Paint.exe. This image is then used as a basic video
frame in making this 2-second video from Mac iStopMotion.

#### four-colors-aspect-4x3.mp4
Actual video frames are the same as four-colors.mp4, except it specifies
an aspect of 4x3 in mp4 meta data.

#### four-colors-aspect-rot-90.mp4
Actual video frames are the same as four-colors.mp4, except it specifies
a rotation of 90 degrees in mp4 meta data.

#### four-colors-aspect-rot-180.mp4
Actual video frames are the same as four-colors.mp4, except it specifies
a rotation of 180 degrees in mp4 meta data.

#### four-colors-aspect-rot-270.mp4
Actual video frames are the same as four-colors.mp4, except it specifies
a rotation of 270 degrees in mp4 meta data.

#### four-colors-vp9.webm
A 960x540 vp9 video with 4 color blocks (Y,R,G,B) in every frame. This is
converted from four-colors.mp4 by ffmpeg.

#### four-colors-vp9-i420a.webm
A 960x540 yuva420p vp9 video with 4 color blocks (Y,R,G,B) in every frame. This
is converted from four-colors.mp4 by adding an opacity of 0.5 using ffmpeg.

#### bear-320x180-hi10p.mp4
#### bear-320x240-vp9_profile2.webm
VP9 encoded video with profile 2 (10-bit, 4:2:0).
Codec string: vp09.02.10.10.01.02.02.02.00.

### AAC test data from MPEG-DASH demoplayer (44100 Hz, stereo)
Duration of each packet is (1024/44100 Hz), approximately 23.22 ms.

* aac-44100-packet-0  - timestamp: 0ms
* aac-44100-packet-1  - timestamp: 23.22ms
* aac-44100-packet-2  - timestamp: 46.44ms
* aac-44100-packet-3  - timestamp: 69.66ms

### Vorbis test data from bear.ogv (44100 Hz, 16 bits, stereo)

* vorbis-extradata - Vorbis extradata section
* vorbis-packet-0  - timestamp: 0ms, duration: 0ms
* vorbis-packet-1  - timestamp: 0ms, duration: 0ms
* vorbis-packet-2  - timestamp: 0ms, duration: 0ms
* vorbis-packet-3  - timestamp: 2902ms, duration: 0ms

### MSE MP4 keyframe-metadata versus encoded AVC keyframe-ness test media

#### bear-640x360-v-2frames_frag.mp4
Just first 2 video frames of bear-640x360-v_frag.mp4, created with:
```
ffmpeg -i bear-640x360-v_frag.mp4 -vcodec copy -movflags frag_keyframe+empty_moov+default_base_moof \
    -vframes 2 bear-640x360-v-2frames_frag.mp4
```
It's 1 keyframe + 1 non-keyframe, with container's frame keyframe-ness correct.

#### bear-640x360-v-2frames-keyframe-is-non-sync-sample_frag.mp4
This is bear-640x360-v-2frames_frag.mp4, with manually updated
trun.first_sample_flags: s/0x02000000/0x01010000 (first frame is
non-sync-sample, depends on another frame, mismatches compressed h264 first
frame's keyframe-ness).

#### bear-640x360-v-2frames-nonkeyframe-is-sync-sample_frag.mp4
This is bear-640x360-v-2frames_frag.mp4, with manually updated
tfhd.default_sample_flags: s/0x01010000/0x02000000 (second frame is sync-sample,
doesn't depend on other frames, mismatches compressed h264 second frame's
nonkeyframe-ness).

## Encrypted Test Files

[Shaka Packager]: https://github.com/google/shaka-packager

[1] Test key ID: 30313233343536373839303132333435

[2] Test key: ebdd62f16814d27b68ef122afce4ae3c

[3] KeyIds and Keys are created by left rotating key ID [1] and key [2] using
    std::rotate for every new crypto period. This is only for testing. The
    actual key rotation algorithm is often much more complicated.

### General

#### bear-1280x720-a_frag-cenc.mp4
A fragmented MP4 version of the audio track of bear-1280x720.mp4 encrypted
(ISO CENC) using key ID [1] and key [2].

#### bear-1280x720-a_frag-cenc-key_rotation.mp4
A fragmented MP4 version of the audio track of bear-1280x720.mp4 encrypted
(ISO CENC) using key ID [1] and key [2] with key rotation [3].

#### bear-1280x720-a_frag-cenc_clear-all.mp4
Same as bear-1280x720-a_frag-cenc.mp4 but no fragments are encrypted.

#### bear-1280x720-v_frag-cenc.mp4
A fragmented MP4 version of the video track of bear-1280x720.mp4 encrypted
(ISO CENC) using key ID [1] and key [2].

#### bear-1280x720-v_frag-cenc-key_rotation.mp4
A fragmented MP4 version of the video track of bear-1280x720.mp4 encrypted
(ISO CENC) using key ID [1] and key [2] with key rotation [3].

#### bear-1280x720-v_frag-cenc_clear-all.mp4
Same as bear-1280x720-v_frag-cenc.mp4 but no fragments are encrypted.

#### bear-1280x720-a_frag-cenc_missing-saiz-saio.mp4
An invalid file similar to bear-1280x720-a_frag-cenc.mp4 but has no saiz and
saio boxes. To save space, it has only one encrypted sample.

#### bear-320x240-v_frag-vp9.mp4
Bear video with VP9 codec in MP4 container. Generated with [Shaka Packager] at
1e2da22c8809c17cc4dfdb45924ec45e42058393:
```
packager in=bear-vp9.webm,stream=video,out=bear-320x240-v_frag-vp9.mp4
```

#### bear-320x240-v_frag-vp9-cenc.mp4
Same as above, with video encrypted using key ID [1] and key [2]. Generated with
[Shaka Packager] at 1e2da22c8809c17cc4dfdb45924ec45e42058393:
```
packager in=bear-vp9.webm,stream=video,out=bear-320x240-v_frag-vp9-cenc.mp4
         --enable_fixed_key_encryption --key_id 30313233343536373839303132333435
         --key ebdd62f16814d27b68ef122afce4ae3c --clear_lead 0
         --pssh 0000003470737368010000001077EFECC0B24D02ACE33C1E52E2FB4B000000013031323334353637383930313233343500000000000000467073736800000000EDEF8BA979D64ACEA3C827DCD51D21ED000000261210303132333435363738393031323334351A00221030313233343536373839303132333435
```

#### bear-320x240-16x9-aspect-av_enc-av.webm
bear-320x240-16x9-aspect.webm with audio & video encrypted using key ID [1] and
key [2]

#### bear-320x240-av_enc-av.webm
bear-320x240.webm with audio & video encrypted using key ID [1] and key [2].

#### bear-320x240-av_enc-av_clear-1s.webm
Same as bear-320x240-av_enc-av.webm but with no frames in the first second
encrypted.

#### bear-320x240-av_enc-av_clear-all.webm
Same as bear-320x240-av_enc-av.webm but with no frames encrypted.

#### bear-320x240-v-vp9_profile2_subsample_cenc-v.webm
Encrypted Bear video with VP9 codec (profile 2) in WebM container, using key ID
[1] and key [2]. Codec string: `vp09.02.10.10.01.02.02.02.00`.
Generated with [Shaka Packager] at 4ba5bec66054cfd4af13c07ac62a97f1a1a2e5f9:
```
packager in=bear-320x240-vp9_profile2.webm,stream=video,out=bear-320x240-v-vp9_profile2_subsample_cenc-v.webm
         --enable_fixed_key_encryption --key_id 30313233343536373839303132333435
         --key ebdd62f16814d27b68ef122afce4ae3c --clear_lead 0
         --pssh 0000003470737368010000001077EFECC0B24D02ACE33C1E52E2FB4B000000013031323334353637383930313233343500000000000000467073736800000000EDEF8BA979D64ACEA3C827DCD51D21ED000000261210303132333435363738393031323334351A00221030313233343536373839303132333435
```

#### bear-320x240-v-vp9_profile2_subsample_cenc-v.mp4
Same as above, in MP4 container. Codec string: vp09.02.10.10.01.02.02.02.00.
Generated with [Shaka Packager] at 4ba5bec66054cfd4af13c07ac62a97f1a1a2e5f9:
```
packager in=bear-320x240-vp9_profile2.webm,stream=video,out=bear-320x240-v-vp9_profile2_subsample_cenc-v.mp4
         --enable_fixed_key_encryption --key_id 30313233343536373839303132333435
         --key ebdd62f16814d27b68ef122afce4ae3c --clear_lead 0
         --pssh 0000003470737368010000001077EFECC0B24D02ACE33C1E52E2FB4B000000013031323334353637383930313233343500000000000000467073736800000000EDEF8BA979D64ACEA3C827DCD51D21ED000000261210303132333435363738393031323334351A00221030313233343536373839303132333435
```

#### bear-640x360-av_enc-av.webm
bear-640x360.webm with audio & video encrypted using key ID [1] and key [2].

#### bear-320x240-av_enc-v.webm
bear-320x240.webm with video track encrypted using key ID [1] and key [2].

#### bear-320x240-av_enc-a.webm
bear-320x240.webm with audio track encrypted using key ID [1] and key [2].

#### bear-320x240-v_enc-v.webm
bear-320x240-video-only.webm encrypted using key ID [1] and key [2].

#### bear-320x240-v-vp9_fullsample_enc-v.webm
bear-vp9.webm VP9 video only encrypted using key ID [1] and key [2] with full
sample encryption.

#### bear-320x240-v-vp9_subsample_enc-v.webm
bear-vp9.webm VP9 video only encrypted using key ID [1] and key [2] with
[subsample encryption](http://www.webmproject.org/docs/webm-encryption/#46-subsample-encrypted-block-format).

#### bear-320x240-opus-a_enc-a.webm
bear-opus.webm encrypted using key ID [1] and key [2].

#### bear-320x240-opus-av_enc-av.webm
bear-vp9-opus.webm with audio & video encrypted using key ID [1] and key [2].

#### bear-320x240-opus-av_enc-v.webm
bear-vp9-opus.webm with video track encrypted using key ID [1] and key [2].

#### bear-640x360-a_frag-cenc.mp4
A fragmented MP4 version of the audio track of bear-640x360.mp4 encrypted (ISO
CENC) using key ID [1] and key [2].

**Note**: bear-640x360.mp4 file does not exist any more. Files encrypted from
it has AAC audio and H264 high profile video (if applicable).

#### bear-640x360-a_frag-cenc-key_rotation.mp4
A fragmented MP4 version of the audio track of bear-640x360.mp4 encrypted (ISO
CENC) using key ID [1] and key [2] with key rotation [3].

#### bear-640x360-v_frag-cenc-mdat.mp4
A fragmented MP4 version of the video track of bear-640x360.mp4 encrypted (ISO
CENC) using key ID [1] and key [2]  and with sample encryption auxiliary
information in the beginning of mdat box.

#### bear-640x360-v_frag-cenc-senc.mp4
Same as above, but with sample encryption information stored in SampleEncryption
('senc') box.

#### bear-640x360-v_frag-cenc-senc-no-saiz-saio.mp4
Same as above, but without saiz and saio boxes.

#### bear-640x360-v_frag-cenc-key_rotation.mp4
A fragmented MP4 version of the video track of bear-640x360.mp4 encrypted (ISO
CENC) using key ID [1] and key [2] with key rotation [3].

#### bear-640x360-a_frag-cbcs.mp4
- Same as previous instructions, except source is bear-640x360-a_frag.mp4
  and no clear lead (i.e. --clear_lead 0).

#### bear-flac-cenc.mp4
Encrypted version of bear-flac.mp4, encrypted by [Shaka Packager] v2.1.0 using
key ID [1] and key [2]. Sample encryption information stored in SampleEncryption
('senc') box (in each encrypted fragment).

```
packager in=bear-flac.mp4,stream=audio,output=bear-flac-cenc.mp4
         --enable_raw_key_encryption
         --clear_lead 0.5
         --segment_duration 0.5
         --keys label=:key_id=30313233343536373839303132333435:key=ebdd62f16814d27b68ef122afce4ae3c
         --pssh 000000327073736800000000EDEF8BA979D64ACEA3C827DCD51D21ED000000121210303132333435363738393031323334350000003470737368010000001077EFECC0B24D02ACE33C1E52E2FB4B000000013031323334353637383930313233343500000000
```

#### bear-opus-cenc.mp4
Encrypted version of bear-opus.mp4, encrypted by [Shaka Packager] v2.3.0 using
key ID [1] and key [2].

```
packager in=bear-opus.mp4,stream=audio,output=bear-opus-cenc.mp4
         --enable_raw_key_encryption
         --protection_scheme cenc
         --clear_lead 0
         --segment_duration 0.5
         --keys label=:key_id=30313233343536373839303132333435:key=ebdd62f16814d27b68ef122afce4ae3c
         --pssh 000000327073736800000000EDEF8BA979D64ACEA3C827DCD51D21ED000000121210303132333435363738393031323334350000003470737368010000001077EFECC0B24D02ACE33C1E52E2FB4B000000013031323334353637383930313233343500000000
```

#### bear-a_enc-a.webm
bear-320x240-audio-only.webm encrypted using key ID [1] and key [2].

#### frame_size_change-av_enc-v.webm
third_party/WebKit/LayoutTests/media/resources/frame_size_change.webm encrypted
using key ID [1] and key [2].

### AV1

Unless noted otherwise, the codec string is `av01.0.04M.08` for 8-bit files,
and `av01.0.04M.10` for 10-bit files.

#### bear-av1-cenc.mp4
Encrypted version of bear-av1.mp4. Encrypted by [Shaka Packager] built locally
at commit 53aa775ea488c0ffd3a2e1cb78ad000154e414e1 using key ID [1] and key [2].
```
packager in=bear-av1.mp4,stream=video,output=bear-av1-cenc.mp4
         --enable_raw_key_encryption --protection_scheme cenc --clear_lead 0
         --keys label=:key_id=30313233343536373839303132333435:key=ebdd62f16814d27b68ef122afce4ae3c
         --pssh 000000327073736800000000EDEF8BA979D64ACEA3C827DCD51D21ED000000121210303132333435363738393031323334350000003470737368010000001077EFECC0B24D02ACE33C1E52E2FB4B000000013031323334353637383930313233343500000000
```

#### bear-av1-cenc.webm
Same as bear-av1-cenc.mp4, except that the output name is bear-av1-cenc.webm.

#### bear-av1-320x180-10bit-cenc.mp4
Same as bear-av1-cenc.mp4, except that the input name is
bear-av1-320x180-10bit.mp4, and the output name is
bear-av1-320x180-10bit-cenc.mp4.

#### bear-av1-320x180-10bit-cenc.webm
Same as bear-av1-cenc.mp4, except that the input name is
bear-av1-320x180-10bit.webm, and the output name is
bear-av1-320x180-10bit-cenc.webm.

### Encryption Scheme Test

* bear-640x360-v_frag-cenc.mp4
* bear-640x360-v_frag-cbc1.mp4
* bear-640x360-v_frag-cbcs.mp4
* bear-640x360-v_frag-cens.mp4

Encrypted versions of bear-640x360-v_frag.mp4, encrypted by [Shaka Packager]
v2.0.0 using key ID [1] and key [2]. Sample encryption information stored in
SampleEncryption ('senc') box (in each encrypted fragment).

Command: (replace both places of 'cenc' with desired scheme)

```
packager in=bear-640x360-v_frag.mp4,stream=video,output=bear-640x360-v-cenc.mp4
         --enable_raw_key_encryption
         --protection_scheme cenc
         --clear_lead 0.5
         --segment_duration 0.5
         --keys label=:key_id=30313233343536373839303132333435:key=ebdd62f16814d27b68ef122afce4ae3c
         --pssh 000000327073736800000000EDEF8BA979D64ACEA3C827DCD51D21ED000000121210303132333435363738393031323334350000003470737368010000001077EFECC0B24D02ACE33C1E52E2FB4B000000013031323334353637383930313233343500000000
```

The 'pssh' data includes entries for both Widevine and
[Common SystemID](https://w3c.github.io/encrypted-media/format-registry/initdata/cenc.html#common-system).
It was generated from concatenating the output of:
```
shaka/packager/tools/pssh/pssh-box.py --widevine-system-id --key-id 30313233343536373839303132333435 --hex
shaka/packager/tools/pssh/pssh-box.py --common-system-id --key-id 30313233343536373839303132333435 --hex
```

### HLS

#### bear-1280x720-hls.ts
Produced using Apple's mediafilesegmenter tool with bear-1280x720.ts as input,
with no encryption.
```
mediafilesegmenter -t 10 -start-segments-with-iframe -f 'output_clear/' bear-1280x720.ts
```

#### bear-1280x720-hls-sample-aes.ts
Produced using Apple's mediafilesegmenter tool also with bear-1280x720.ts as
input, but with SAMPLE_AES encryption. (Additional TS packets were constructed
manually and prepended to convey the encryption metadata, notably key id and IV).
```
mediafilesegmenter -S -P -k 'key_iv.bin' -t 10 -start-segments-with-iframe -f 'output/' bear-1280x720.ts
```

#### bear-1280x720-hls-with-CAT.ts
Same as bear-1280x720-hls.ts but with an extra TS packet prepended. This is the
same as the first of the metadata packets in bear-1280x720-hls-sample-aes.ts.
Its presence indicates that SAMPLE_AES encryption may occur later in the stream,
and causes the initial audio and video configs to have an encryption_scheme (of
AES-CBC). The actual data is unencrypted, which is indicated by the lack of key
ID and IV. This ends up very similar to how clear leader of an otherwise
encrypted stream can occur in MP4.

## Container Test Files

Additional containers derived from bear.ogv:

* bear.ac3    -- created using `avconv -i bear.ogv -f ac3 -b 192k bear.ac3`.
* bear.adts   -- created using `avconv -i bear.ogv -f adts -strict experimental bear.adts`.
* bear.aiff   -- created using `avconv -i bear.ogv -f aiff bear.aiff`.
* bear.asf    -- created using `avconv -i bear.ogv -f asf bear.asf`.
* bear.avi    -- created using `avconv -i bear.ogv -f avi -b 192k bear.avi`.
* bear.eac3   -- created using `avconv -i bear.ogv -f eac3 bear.eac3`.
* bear.flac   -- created using `avconv -i bear.ogv -f flac bear.flac`.
* bear.flv    -- created using `avconv -i bear.ogv -f flv bear.flv`.
* bear.h261   -- created using `avconv -i bear.ogv -f h261 -s:0 cif bear.h261`.
* bear.h263   -- created using `avconv -i bear.ogv -f h263 -s:0 cif bear.h263`.
* bear.m2ts   -- created using `avconv -i bear.ogv -f mpegts bear.m2ts`.
* bear.mjpeg  -- created using `avconv -i bear.ogv -f mjpeg bear.mjpeg`.
* bear.mpeg   -- created using `avconv -i bear.ogv -f mpeg bear.mpeg`.
* bear.rm     -- created using `avconv -i bear.ogv -f rm -b 192k bear.rm`.
* bear.swf    -- created using `avconv -i bear.ogv -f swf -an bear.swf`.

## VDA Test Files:

### test-25fps

#### test-25fps.h264
Using ffmpeg SVN-r0.5.9-4:0.5.9-0ubuntu0.10.04.1 @ WebKit r122718, generated
with:
```
ffmpeg -i third_party/WebKit/LayoutTests/media/content/test-25fps.mp4 \
      -vcodec copy -vbsf h264_mp4toannexb -an test-25fps.h264
```

#### test-25fps.h264.json:
JSON file that contains all metadata related to test-25fps.h264, used by the
video_decode_accelerator_tests. This includes the video codec, resolution and
md5 checksums of individual video frames when converted to the I420 format.

#### test-25fps.vp8
ffmpeg git-2012-07-19-a8d8e86, libvpx ToT 7/19, chromium r147247,
mkvextract v5.0.1
```
ffmpeg -i test-25fps.h264 -vcodec libvpx -an test-25fps.webm && \
    mkvextract tracks test-25fps.webm 1:test-25fps.vp8 && rm test-25fps.webm
```

#### test-25fps.vp8.json:
JSON file that contains all metadata related to test-25fps.vp8, used by the
video_decode_accelerator_tests. This includes the video codec, resolution and
md5 checksums of individual video frames when converted to the I420 format.

#### test-25fps.vp9
avconv 9.16-6:9.16-0ubuntu0.14.04.1, vpxenc v1.3.0
```
avconv -i test-25fps.h264 -c:v rawvideo -pix_fmt yuv420p test-25fps_i420.yuv
vpxenc test-25fps_i420.yuv -o test-25fps.vp9 --codec=vp9 -w 320 -h 240 --ivf \
    --profile=0 --kf-min-dist=0 --kf-max-dist=150 --lag-in-frames=24 \
    --drop-frame=0 --target-bitrate=140 --cq-level=23 --min-q=4 --max-q=56 \
    --static-thresh=1000 --arnr-maxframes=7 --arnr-strength=5 --arnr-type=3 \
    --cpu-used=1 --good --tile-columns=1 --passes=2 --threads=1 --fps=25/1 \
    --end-usage=cq --auto-alt-ref=1 --bias-pct=50 --minsection-pct=0 \
    --maxsection-pct=2000 --undershoot-pct=100
```

#### test-25fps.vp9.json:
JSON file that contains all metadata related to test-25fps.vp9, used by the
video_decode_accelerator_tests. This includes the video codec, resolution and
md5 checksums of individual video frames when converted to the I420 format.

#### test-25fps.vp9_2
Similar to test-25fps.vp9, substituting the option `--profile=0` with
`--profile=2 --bit-depth=10` to vpxenc. (Note that vpxenc must have been
configured with the option --enable-vp9-highbitdepth).

#### test-25fps.vp9_2.json:
JSON file that contains all metadata related to test-25fps.vp9_2, used by the
video_decode_accelerator_tests. This includes the video codec, resolution and
md5 checksums of individual video frames when converted to the I420 format.


### VP9 video with show_existing_frame flag

#### vp90_2_10_show_existing_frame2.vp9.ivf
VP9 video with show_existing_frame flag. The original test stream comes from
Android CTS.
```
ffmpeg -i vp90_2_17_show_existing_frame.vp9 -vcodec copy -an -f ivf \
    vp90_2_17_show_existing_frame.vp9.ivf
```


### bear

#### bear.h264
Using ffmpeg version 0.8.6-4:0.8.6-0ubuntu0.12.04.1, generated with
bear.mp4 (https://chromiumcodereview.appspot.com/10805089):
```
ffmpeg -i bear.mp4 -vcodec copy -vbsf h264_mp4toannexb -an bear.h264
```

### npot-video

#### npot-video.h264
Using ffmpeg version 0.8.6-4:0.8.6-0ubuntu0.12.04.1, generated with
npot-video.mp4 (https://codereview.chromium.org/8342021):
```
ffmpeg -i npot-video.mp4 -vcodec copy -vbsf h264_mp4toannexb -an npot-video.h264
```

### red-green

#### red-green.h264
Using ffmpeg version 0.8.6-4:0.8.6-0ubuntu0.12.04.1, generated with
red-green.mp4 (https://codereview.chromium.org/8342021):
```
ffmpeg -i red-green.mp4 -vcodec copy -vbsf h264_mp4toannexb -an red-green.h264
```

## Misc Test Files

### resolution_change_500frames

#### resolution_change_500frames-vp8.ivf
#### resolution_change_500frames-vp9.ivf
Dumped compressed stream of videos on
[http://crosvideo.appspot.com](http://crosvideo.appspot.com) manually
changing resolutions at random. Those contain 144p, 240p, 360p, 480p, 720p, and
1080p frames. Those frame sizes can be found by
```
ffprobe -show_frames resolution_change_500frames.vp8
```

#### switch_1080p_720p_240frames
#### switch_1080p_720p_240frames.h264
Extract 240 frames using ffmpeg from
http://commondatastorage.googleapis.com/chromiumos-test-assets-public/MSE/switch_1080p_720p.mp4.

The frame sizes change between 1080p and 720p every 24 frames.

### VEA test files:

#### bear_320x192_40frames.yuv.webm
First 40 raw i420 frames of bear-1280x720.mp4 scaled down to 320x192 for
video_encode_accelerator_unittest. Encoded with vp9 lossless:
`ffmpeg -pix_fmt yuv420p -s:v 320x192 -r 30 -i bear_320x192_40frames.yuv -lossless 1 bear_320x192_40frames.yuv.webm`

#### bear_640x384_40frames.yuv.webm
First 40 raw i420 frames of bear-1280x720.mp4 scaled down to 340x384 for
video_encode_accelerator_unittest. Encoded with vp9 lossless:
`ffmpeg -pix_fmt yuv420p -s:v 640x384 -r 30 -i bear_640x384_40frames.yuv -lossless 1 bear_640x384_40frames.yuv.webm`


### ImageProcessor Test Files

#### bear\_320x192.i420.yuv
First frame of bear\_320x192\_40frames.yuv for image\_processor_test.

#### bear\_320x192.i420.yuv.json
Metadata describing bear\_320x192.i420.yuv.

#### bear\_320x192.nv12.yuv
First frame of bear\_320x192\_40frames.nv12.yuv for image\_processor_test.

#### bear\_320x192.nv12.yuv.json
Metadata describing bear\_320x192.nv12.yuv.

#### bear\_320x192.yv12.yuv
First frame of bear\_320x192\_40frames.yv12.yuv for image\_processor_test.

#### bear\_320x192.rgba
RAW RGBA format data. This data is created from bear\_320x192.i420.yuv by the
following command. Alpha channel is always 0xFF because of that.
`ffmpeg -s 320x192 -pix_fmt yuv420p -i bear_320x192.i420.yuv -vcodec rawvideo -f image2 -pix_fmt rgba bear_320x192.rgba`

#### bear\_320x192.bgra
RAW BGRA format data. This data is created from bear\_320x192.i420.yuv by the
following command. Alpha channel is always 0xFF because of that.
`ffmpeg -s 320x192 -pix_fmt yuv420p -i bear_320x192.i420.yuv -vcodec rawvideo -f image2 -pix_fmt rgba bear_320x192.bgra`


#### puppets-1280x720.nv12.yuv
RAW NV12 format data. The width and height are 1280 and 720, respectively.
This data is created from peach\_pi-1280x720.jpg by the following command.
`ffmpeg -i peach_pi-1280x720.jpg -s 1280x720 -pix_fmt nv12 puppets-1280x720.nv12.yuv`

#### puppets-640x360.nv12.yuv
RAW NV12 format data. The width and height are 640 and 360, respectively.
This data is created from puppets-1280x720.nv12.yuv by the following command.
`ffmpeg -s:v 1280x720 -pix_fmt nv12 -i puppets-1280x720.nv12.yuv -vf scale=640x360 -c:v rawvideo -pix_fmt nv12 puppets-640x360.nv12.yuv`

#### puppets-320x180.nv12.yuv
RAW NV12 format data. The width and height are 320 and 180, respectively.
This data is created from puppets-1280x720.nv12.yuv by the following command.
`ffmpeg -s:v 1280x720 -pix_fmt nv12 -i puppets-1280x720.nv12.yuv -vf scale=320x180 -c:v rawvideo -pix_fmt nv12 puppets-320x180.nv12.yuv`

###  VP9 parser test files:

#### bear-vp9.ivf
Created using "avconv -i bear-vp9.webm -vcodec copy -an -f ivf bear-vp9.ivf".

#### bear-vp9.ivf.context

#### test-25fps.vp9.context
Manually dumped from libvpx with bear-vp9.ivf and test-25fps.vp9. See
vp9_parser_unittest.cc for description of their format.

###  WebM files for testing multiple tracks.

#### green-a300hz.webm
WebM file containing 12 seconds of solid green video + 300Hz sine wave audio

#### red-a500hz.webm
WebM file containing 10 seconds of solid red video + 500Hz sine wave audio

Created with the following commands:
```
ffmpeg -f lavfi -i color=c=green:size=160x120 -t 12 -c:v libvpx green.webm
ffmpeg -f lavfi -i color=c=red:size=320x240 -t 10 -c:v libvpx red.webm
ffmpeg -f lavfi -i "sine=frequency=300:sample_rate=48000" -t 12 -c:v libvpx a300hz.webm
ffmpeg -f lavfi -i "sine=frequency=500:sample_rate=48000" -t 10 -c:v libvpx a500hz.webm
ffmpeg -i green.webm -i a300hz.webm -map 0 -map 1 green-a300hz.webm
ffmpeg -i red.webm -i a500hz.webm -map 0 -map 1 red-a500hz.webm
```

### WebP Test Files

#### BlackAndWhite_criss-cross_pattern_2015x2015.webp
A lossy WebP encoded image of 2015x2015. Created by gildekel@ using Gimp.

#### bouncy_ball.webp
An animated (extended) WebP encoded image of 450x450. Created by gildekel@
using Gimp.

#### red_green_gradient_lossy.webp
A lossy WebP encoded image of 3000x3000. Created by gildekel@ using Gimp.

#### RGB_noise_2015x2015.webp
A lossy WebP encoded image of 2015x2015 that contains RGB noise. Created by
gildekel@ using Gimp.

#### RGB_noise_large_pixels_115x115.webp
A lossy WebP encoded image of 115x115 that contains large pixels RGB noise.
Created by gildekel@ using Gimp.

#### RGB_noise_large_pixels_2015x2015.webp
A lossy WebP encoded image of 2015x2015 that contains large pixels RGB noise.
Created by gildekel@ using Gimp.

#### RGB_noise_large_pixels_4000x4000.webp
A lossy WebP encoded image of 4000x4000 that contains large pixels RGB noise.
Created by gildekel@ using Gimp.

#### solid_green_2015x2015.webp
A lossy WebP encoded image of 2015x2015 of solid bright green. Created by
gildekel@ using Gimp.

#### yellow_pink_gradient_lossless.webp
A lossless WebP encoded image of 3000x3000. Created by gildekel@ using Gimp.

### JPEG Test Files

#### pixel-1280x720.jpg
Single MJPEG encoded frame of 1280x720, captured on Chromebook Pixel. This image
does not have Huffman table.

#### pixel-1280x720-trailing-zeros.jpg
A version of pixel-1280x720.jpg with five trailing zero bytes after the EOI
marker. The command used to generated it was:
```
echo -e "`xxd -g1 -p -c1 pixel-1280x720.jpg`" "\n00\n00\n00\n00\n00" | xxd -r -g1 -p -c1 > pixel-1280x720-trailing-zeros.jpg
```

#### pixel-1280x720-grayscale.jpg
A version of pixel-1280x720.jpg converted to grayscale using:
```
jpegtran -grayscale pixel-1280x720.jpg > pixel-1280x720-grayscale.jpg
```
Then, using a hex editor, the Huffman table sections were removed from the
resulting file.

#### pixel-1280x720-yuv420.jpg
A version of pixel-1280x720.jpg converted to 4:2:0 subsampling using:
```
convert pixel-1280x720.jpg -sampling-factor 4:2:0 -define jpeg:optimize-coding=false pixel-1280x720-yuv420.jpg
```
Then, using a hex editor, the Huffman table sections were removed from the
resulting file.

#### pixel-40x23-yuv420.jpg
A version of pixel-1280x720-yuv420.jpg resized to 40x23 (so that the height is
odd) using:
```
convert pixel-1280x720-yuv420.jpg -resize 40x23\! -define jpeg:optimize-coding=false pixel-40x23-yuv420.jpg
```
Then, using a hex editor, the Huffman table sections were removed from the
resulting file.

#### pixel-41x22-yuv420.jpg
A version of pixel-1280x720-yuv420.jpg resized to 41x22 (so that the width is
odd) using:
```
convert pixel-1280x720-yuv420.jpg -resize 41x22\! -define jpeg:optimize-coding=false pixel-41x22-yuv420.jpg
```
Then, using a hex editor, the Huffman table sections were removed from the
resulting file.

#### pixel-41x23-yuv420.jpg
A version of pixel-1280x720-yuv420.jpg resized to 41x23 (so that both dimensions
are odd) using:
```
convert pixel-1280x720-yuv420.jpg -resize 41x23\! -define jpeg:optimize-coding=false pixel-41x23-yuv420.jpg
```
Then, using a hex editor, the Huffman table sections were removed from the
resulting file.

#### pixel-1280x720-yuv444.jpg
A version of pixel-1280x720.jpg converted to 4:4:4 subsampling using:
```
convert pixel-1280x720.jpg -sampling-factor 4:4:4 -define jpeg:optimize-coding=false pixel-1280x720-yuv444.jpg
```
Then, using a hex editor, the Huffman table sections were removed from the
resulting file.

#### peach_pi-1280x720.jpg
Single MJPEG encoded frame of 1280x720, captured on Samsung Chromebook 2(13").
This image has Huffman table.

#### blank-1x1.jpg
1x1 small picture to test special cases.

### MP4 files with non-square pixels.

#### bear-640x360-non_square_pixel-with_pasp.mp4
Size in TKHD is (639.2x360) and size in STSD is (470x360). A PASP box is
present with hSpacing=34 and vSpacing=25. Note that 470.0 * 34 / 25 = 639.2.

#### bear-640x360-non_square_pixel-without_pasp.mp4
Size in TKHD is (639.2x360) and size in STSD is (470x360). No PASP box is
present.

### MP4 files with AC3 and EAC3 audio

#### bear-ac3-only-frag.mp4
AC3 audio in framented MP4, generated with
```
ffmpeg -i bear.ac3 -acodec copy -movflags frag_keyframe bear-ac3-only-frag.mp4
```

#### bear-eac3-only-frag.mp4
EAC3 audio in framented MP4, generated with
```
ffmpeg -i bear.eac3 -acodec copy -movflags frag_keyframe bear-eac3-only-frag.mp4
```

### Mpeg2ts stream with AAC HE audio that uses SBR

#### bear-1280x720-aac_he.ts
Generated by the following command:
```
ffmpeg -i bear-1280x720.mp4 -c:v libx264 -c:a libfdk_aac -profile:a aac_he  bear-1280x720-aac_he.ts
```

### Mpeg2ts streams MP3 audio

#### bear-audio-mp4a.6B.ts
Generated by the following commands:
```
ffmpeg -i bear_pcm.wav -c:a mp3 -ar 44100 bear-audio-mp4a.6B.ts
```

#### bear-audio-mp4a.69.ts
Generated by the following commands:
```
ffmpeg -i bear_pcm.wav -c:a mp3 -ar 22050 bear-audio-mp4a.69.ts
```

### MP4 file with HEVC

#### bear-320x240-v_frag-hevc.mp4
HEVC video stream in fragmented MP4 container, generated with
```
ffmpeg -i bear-320x240.webm -c:v libx265 -an -movflags faststart+frag_keyframe bear-320x240-v_frag-hevc.mp4
```

#### bear-320x240-v-2frames_frag-hevc.mp4
HEVC video stream in fragmented MP4 container, including the first 2 frames, generated with
```
ffmpeg -i bear-320x240.webm -c:v libx265 -an -movflags frag_keyframe+empty_moov+default_base_moof \
    -vframes 2  bear-320x240-v-2frames_frag-hevc.mp4
```

#### bear-320x240-v-2frames-keyframe-is-non-sync-sample_frag-hevc.mp4
This is bear-320x240-v-2frames_frag-hevc.mp4, with manually updated
trun.first_sample_flags: s/0x02000000/0x01010000 (first frame is
non-sync-sample, depends on another frame, mismatches compressed h265 first
frame's keyframe-ness).

#### bear-320x240-v-2frames-nonkeyframe-is-sync-sample_frag-hevc.mp4
This is bear-320x240-v-2frames_frag-hevc.mp4, with manually updated
tfhd.default_sample_flags: s/0x01010000/0x02000000 (second frame is sync-sample,
doesn't depend on other frames, mismatches compressed h265 second frame's
nonkeyframe-ness).

### Multi-track MP4 file

(c) copyright 2008, Blender Foundation / www.bigbuckbunny.org

#### bbb-320x240-2video-2audio.mp4

Generated using following steps:

1.  Download the source file with 1 video and 1 audio stream.
    ```
    wget http://distribution.bbb3d.renderfarming.net/video/mp4/bbb_sunflower_1080p_30fps_normal.mp4
    ```
2.  Generate a scaled down to 320x240 video + 2 channel AAC LC audio from the
    source file.
    ```
    ffmpeg -i bbb_sunflower_1080p_30fps_normal.mp4 -c:v libx264 -crf 36 -vf  scale=320:240 -c:a libfdk_aac -ac 2 -t 24 bbb1.mp4
    ```
3.  Generate a file with the original video scaled down to 320x240 and flipped
    upside down and sine wave instead of audio.
    ```
    ffmpeg -i bbb_sunflower_1080p_30fps_normal.mp4 -f lavfi -i "sine=frequency=500:sample_rate=48000" -map 0:v -map 1:a -c:v libx264 -crf 36 -vf scale=320:240,vflip -c:a libfdk_aac -ac 2 -t 24 bbb2.mp4
    ```
4.  Combine the two files generated above into a single fragmented .mp4 file
    with 2 video and 2 audio tracks.
    ```
    ffmpeg -i bbb1.mp4 -i bbb2.mp4 -map 0:0 -map 0:1 -map 1:0 -map 1:1 -c:v copy -c:a copy -movflags frag_keyframe+omit_tfhd_offset+separate_moof bbb-320x240-2video-2audio.mp4
    ```

### Multi-track WebM file

#### multitrack-3video-2audio.webm

Generated using following commands:
```
ffmpeg -f lavfi -i color=c=red:size=320x240 -t 5 -c:v libvpx red.webm
ffmpeg -f lavfi -i color=c=green:size=320x240 -t 5 -c:v libvpx green.webm
ffmpeg -f lavfi -i color=c=blue:size=160x120 -t 10 -c:v libvpx blue.webm
ffmpeg -f lavfi -i "sine=frequency=300:sample_rate=48000" -t 10 -c:v libvpx a300hz.webm
ffmpeg -f lavfi -i "sine=frequency=500:sample_rate=48000" -t 5 -c:v libvpx a500hz.webm
ffmpeg -i red.webm -i green.webm -i blue.webm -i a300hz.webm -i a500hz.webm -map 0 -map 1 -map 2 -map 3 -map 4  multitrack-3video-2audio.webm
```

### Opus pre-skip and end-trimming test clips
https://people.xiph.org/~greg/opus_testvectors/

* opus-trimming-test.mp4
* opus-trimming-test.ogg
* opus-trimming-test.webm
