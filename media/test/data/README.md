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

#### bear-1280x720.ivf

VP8 video stream from bear-1280x720.mp4 in ivf container.

#### noise-xhe-aac.mp4
#### noise-xhe-aac-mono.mp4
Fragmented mp4 of noise encoded with xHE-AAC, from xHE-AAC samples in [Android
CTS](https://android.googlesource.com/platform/cts/+/master/tests/tests/media/decoder/res/raw),
using ffmpeg version 4.2.2 (where nofillin lets audio nonkeyframes in input be
indicated the same in output, unlike more recent tip-of-tree ffmpeg's operation
with this option) to remux, unfortunately with empty MOOV not giving real
duration:
```
ffmpeg -fflags nofillin -i noise_2ch_48khz_aot42_19_lufs_mp4.m4a -acodec copy -t 1 -movflags frag_keyframe+empty_moov+default_base_moof noise-xhe-aac.mp4
ffmpeg -fflags nofillin -i noise_1ch_29_4khz_aot42_19_lufs_drc_config_change_mp4.m4a -acodec copy -t 1 -movflags frag_keyframe+empty_moov+default_base_moof noise-xhe-aac-mono.mp4
ffmpeg -fflags nofillin -i noise_2ch_44_1khz_aot42_19_lufs_config_change_mp4.m4a -acodec copy -t 1 -movflags frag_keyframe+empty_moov+default_base_moof noise-xhe-aac-44kHz.mp4
```

#### sync2-trimmed.mp4
Special mp4 created with audio/video offset by 3 seconds. sync2.mp4 is an
internal higher resolution version of sync2.ogv.
```
ffmpeg -ss 83.482733 -i sync2.mp4 -an -vcodec copy -t 5 out_vid.mp4
ffmpeg -ss 80.482733 -i sync2.mp4 -vn -acodec copy -t 8 out_audio.mp4
ffmpeg -i out_vid.mp4 -itsoffset -3 -i out_audio.mp4 -c copy sync2-trimmed.mp4
```

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

### VVC

#### bear_180p.vvc
Created using FFmpeg/vvencapp with the following commands:
```
ffmpeg -i bear.y4m -f rawvideo bear_180P.yuv
vvencapp --preset medium -i bear_180P.yuv -s 320x180 -r 15 -b 1000000 \
  -p 2 -o bear_180p.vvc
```

#### bbb_360p.vvc
Created using FFmpeg/vvencapp with the following commands:
```
ffmpeg -i bbb-320x240-2video-2audio.mp4 bbb.y4m
ffmpeg -i bbb.y4m -vf scale=480x360 bbb_480x360.yuv
vvencapp --preset medium -i bbb_480x360.yuv -s 480x360 -r 15 -b 1000000 \
  -p 2 -f 60 -o bbb_360p.vvc
```

#### basketball_2_layers.vvc
2 spatial layer VVC video with layer 0 at 208x120 and layer 1 at 832x480.
Used for vvc parser test. Once vvencapp supports multi-layer encoding, the
creation command needs to be provided.

#### bbb_9tiles.vvc
VVC stream generated with VTM with each picture configured to be with 9 tiles
and single raster scan slice.
Created with VTM and ffmpeg:
```
ffmpeg -i bbb-320x240-2video-2audio.mp4 bbb.y4m
ffmpeg -i bbb.y4m -vf scale=1920x1080 bbb_1920x1080.yuv
EncoderApp --EnablePicPartitioning=1 --CTUSize=128 --TileColumnWidthArray=5 \
  --TileRowHeightArray=3 --RasterScanSlices=1 --RasterSliceSizes=15 \
  -f 8 --InputFile=bbb_1920x1080.yuv --BitstreamFile=bbb_9tiles.vvc
```

#### bbb_15tiles_15slices.vvc
VVC stream generated with VTM with each picture configured to be with 15 tiles
and 15 rect slices.
Created with VTM and ffmpeg:
```
ffmpeg -i bbb-320x240-2video-2audio.mp4 bbb.y4m
ffmpeg -i bbb.y4m -vf scale=1920x1080 bbb_1920x1080.yuv
EncoderApp -f 8 --EnablePicPartitioning=1 --CTUSize=128 \
  --TileColumnWidthArray=3,3,3,3,3  --TileRowHeightArray=3,3 \
  --RasterScanSlices=0 --RectSliceFixedWidth=1 \
  --RectSliceFixedHeight=1 --InputFile=bbb_1920x1080.yuv \
  --BitstreamFile=bbb_15tiles_15slices.vvc
```

#### bbb_9tiles_18slices.vvc
VVC stream generated with VTM with each picture configured to be with 9 tiles
and 18 rect slices.
Created with VTM and ffmpeg:
```
ffmpeg -i bbb-320x240-2video-2audio.mp4 bbb.y4m
ffmpeg -i bbb.y4m -vf scale=1920x1080 bbb_1920x1080.yuv
EncoderApp -f 8 --EnablePicPartitioning=1 --CTUSize=128 \
  --TileColumnWidthArray=5,5,5 --TileRowHeightArray=3,3 --RasterScanSlices=0 \
  --RectSliceFixedWidth=0 --RectSliceFixedHeight=0 \
  --RectSlicePositions=0,19,30,34,45,64,75,79,90,109,120,124,5,24,35,39,50,69,\
80,84,95,114,125,129,10,29,40,44,55,74,85,89,100,119,130,134 \
  --InputFile=bbb_1920x1080.yuv --BitstreamFile=bbb_9tiles_18slices.vvc
```

#### bbb_chroma_qp_offset_lists.vvc
VVC stream generated with VTM with chroma QP offset lists enabled.
Created with VTM and ffmpeg:
```
ffmpeg -i bbb-320x240-2video-2audio.mp4 bbb.y4m
ffmpeg -i bbb.y4m -vf scale=1920x1080 bbb_1920x1080.yuv
EncoderApp --CTUSize=128 --CbQpOffsetList=3,3,8,9 --CrQpOffsetList=2,4,1,7 \
  -f 8 --InputFile=bbb_1920x1080.yuv \
  --BitstreamFile=bbb_chroma_qp_offset_lists.vvc
```

#### bbb_scaling_lists.vvc
VVC stream generated with VTM that is used to verify scaling list parsing.
Created with VTM and ffmpeg:
```
ffmpeg -i bbb-320x240-2video-2audio.mp4 bbb.y4m
ffmpeg -i bbb.y4m -vf scale=1920x1080 bbb_1920x1080.yuv
EncoderApp --CTUSize=128 -c sample_scaling_list.cfg -f 8 \
  --InputFile=bbb_1920x1080.yuv \
  --BitstreamFile=bbb_scaling_lists.vvc
```
Please be noted sample_scaling_list.cfg is the file with the same name from VTM
sample configure.

#### bbb_rpl_in_ph_nut.vvc
VVC stream generated with VTM that is used to verify parsing of complex picture
header structure in a separate PH NALU.
Created with VTM and ffmpeg:
```
ffmpeg -i bbb-320x240-2video-2audio.mp4 bbb.y4m
ffmpeg -i bbb.y4m -vf scale=1920x1080 bbb_1920x1080.yuv
EncoderApp -f 5 --EnablePicPartitioning=1 --CTUSize=128 \
  --TileColumnWidthArray=5,5,5 --TileRowHeightArray=3,3 --RasterScanSlices=0 \
  --RectSliceFixedWidth=0 --RectSliceFixedHeight=0 \
  --RectSlicePositions=0,19,30,34,45,64,75,79,90,109,120,124,5,24,35,39,50,69,\
80,84,95,114,125,129,10,29,40,44,55,74,85,89,100,119,130,134 \
  --SliceLevelRpl=0 --SliceLevelDblk=0 --SliceLevelSao=0 --SliceLevelAlf=0 \
  --SliceLevelDeltaQp=0 --ALF=1 --JointCbCr=1 --SAO=1 --WeightedPredP=1 \
  --WeightedPredB=1 --WeightedPredMethod=4 --CCALF=1 \
  --VirtualBoundariesPresentInSPSFlag=0 --SliceLevelWeightedPrediction=0
```

#### bbb_rpl_in_slice.vvc
VVC stream generated with VTM that is used to verify slice header which contains
RPL and pred_weight_table directly in it, instead of the picture header structure
that is part of slice header.
Created with VTM and ffmpeg:
```
ffmpeg -i bbb-320x240-2video-2audio.mp4 bbb.y4m
ffmpeg -i bbb.y4m -vf scale=1920x1080 bbb_1920x1080.yuv
EncoderApp -f 5 --EnablePicPartitioning=1 --CTUSize=128 \
  --SliceLevelRpl=1 --SliceLevelDblk=1 --SliceLevelSao=1 --SliceLevelAlf=1 \
  --SliceLevelDeltaQp=1 --ALF=1 --JointCbCr=1 --SAO=1 --WeightedPredP=1 \
  --WeightedPredB=1 --WeightedPredMethod=4 --CCALF=1 \
  --VirtualBoundariesPresentInSPSFlag=0 --SliceLevelWeightedPrediction=1 \
  --InputFile=bbb_1920x1080.yuv --BitstreamFile=bbb_rpl_in_slice.vvc
```

#### bbb_slice_with_entrypoints.vvc
VVC stream generated with VTM that is used to verify slices with entrypoints
specified in slice header, and also with deblocking filter params in it.
Created with VTM and ffmpeg:
```
ffmpeg -i bbb-320x240-2video-2audio.mp4 bbb.y4m
ffmpeg -i bbb.y4m -vf scale=1920x1080 bbb_1920x1080.yuv
EncoderApp --EnablePicPartitioning=1 --CTUSize=128 --TileColumnWidthArray=5 \
  --TileRowHeightArray=3 --RasterScanSlices=1 --RasterSliceSizes=15 \
  --DisableLoopFilterAcrossTiles=0 --DisableLoopFilterAcrossSlices \
  --SliceLevelRpl=0 --SliceLevelDblk=0 --SliceLevelSao=0 --SliceLevelAlf=0 \
  --SliceLevelWeightedPrediction=0 --VirtualBoundariesPresentInSPSFlag=0 \
  --DeblockingFilterOffsetInPPS=0 --DeblockingFilterDisable=0 \
  --DeblockingFilterBetaOffset_div2=-2 --DeblockingFilterTcOffset_div2=-2 \
  --DeblockingFilterCbBetaOffset_div2=-2 --DeblockingFilterCbTcOffset_div2=0 \
  --DeblockingFilterCrBetaOffset_div2=-2 --DeblockingFilterCrTcOffset_div2=0 \
  --DeblockingFilterMetric=0 --EntryPointsPresent=1 --WaveFrontSynchro=1 \
  -f 2 --InputFile=bbb_1920x1080.yuv --BitstreamFile=bbb_slice_with_entrypoints.vvc
```

#### bbb_2_subpictures_8_slices.vvc
VVC stream generated with VTM that is used to verify VVC slice parser on stream
that is with multiple subpictures.
Created with VTM and ffmpeg:
```
ffmpeg -i bbb-320x240-2video-2audio.mp4 bbb.y4m
ffmpeg -i bbb.y4m -vf scale=1920x1080 bbb_1920x1080.yuv
EncoderApp --EnablePicPartitioning=1 --CTUSize=128 --TileColumnWidthArray=3,4 \
 --TileRowHeightArray=3 --RasterScanSlices=0 --RectSliceFixedWidth=0 \
 --RectSliceFixedHeight=0 --RectSlicePositions=0,17,30,32,45,62,75,77,3,85,90,\
 130,11,89,101,134 --DisableLoopFilterAcrossTiles=1 \
 --DisableLoopFilterAcrossSlices=1 --SubPicInfoPresentFlag=1 --NumSubPics=2 \
 --SubPicCtuTopLeftX=0,11 --SubPicCtuTopLeftY=0,0 --SubPicWidth=11,4 \
 --SubPicHeight=9,9 --SubPicTreatedAsPicFlag=1,1 \
 --LoopFilterAcrossSubpicEnabledFlag=0,0 \
 --SubPicIdMappingExplicitlySignalledFlag=0 \
 -f 2 --InputFile=bbb_1920x1080.yuv --BitstreamFile=bbb_2_subpictures_8slices.vvc
```

#### bbb_poc_msb.vvc
VVC stream generated with VTM that is modified to limit the POC LSB maximum value
to 16. This is done by change sps_log2_max_pic_order_cnt_lsb_minus4 to 0 in the
VTM encoder implementation.

#### bbb_poc_gop8.vvc
VVC stream generated with VTM that is configured to having a GOP size of 4 and
IDR interval of 8, to verify the POC difference for IDR frame with HEVC.
Created with VTM and ffmpeg:
```
ffmpeg -i bbb-320x240-2video-2audio.mp4 bbb_320x240.yuv
EncoderApp --CTUSize=128 --GOPSize=4 --DecodingRefreshType=2 --IntraPeriod=8 \
  --InputFile=bbb_320x240.yuv --BitstreamFile=bbb__poc_gop8.vvc
```

#### vvc_frames_with_ltr.vvc
VVC stream generated manually with modified VTM to enable long term reference
pictures. The stream is also encoded with multiple slices per AU, and each
slice is with different reference picture lists.

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

#### bear-mono-av1.mp4
Similar to the above but using aomenc for encoding with the --monochrome option.

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

#### av1-svc-L1T2.ivf
AV1 data that has temporal layers.
This is the same as av1-1-b8-22-svc-L1T2.ivf in [libaom test vectors].
The video license is [libaom LICENSE].

#### av1-svc-L2T2.ivf
AV1 data that has spatial and temporal layers.
This is the same as av1-1-b8-22-svc-L2T2.ivf in [libaom test vectors].
The video license is [libaom LICENSE].

#### av1-show_existing_frame.ivf
AV1 data that contains frames with `show_existing_frame=1`.
This is the same as 00000592.ivf in
https://people.xiph.org/~tterribe/av1/samples-all/.
The video license is [libaom LICENSE].

#### blackwhite\_yuv444p-frame.av1.ivf
The first frame of blackwhite\_yuv444p.mp4 coded in AV1 by the following command.
`ffmpeg -i blackwhite_yuv444p.mp4 -strict -2 -vcodec av1 -vframes 1 blackwhite_yuv444p-frame.av1.ivf`

#### blackwhite_yuv444p_av1.mp4
`ffmpeg -i blackwhite_yuv444p-frame.av1.ivf -y -vcodec copy blackwhite_yuv444p_av1.mp4`

#### blackwhite_yuv444p_av1.webm
`ffmpeg -i blackwhite_yuv444p-frame.av1.ivf -y -vcodec copy blackwhite_yuv444p_av1.webm`

#### av1-film\_grain.ivf
AV1 data where film grain feature is used.
This is the same as av1-1-b8-23-film\_grain-50.ivf in [libaom test vectors].
The video license is [libaom LICENSE].

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

#### mono_cpe.adts
Technically not corrupt since ffmpeg explicitly allows this stereo track to say
it's mono. First second of audio from test clip on https://crbug.com/1282058.

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

#### four-colors.png
An image (320x240 .png file) of 4 color blocks (Y,R,G,B) is first created by
Windows Paint.exe.

#### four-colors.y4m
A 320x240 raw YUV single frame video with 4 color blocks (Y,R,G,B)
Converted from four-colors.png using ffmpeg:
```
ffmpeg -i four-colors.png  -pix_fmt yuv420p  -f yuv4mpegpipe four-colors.y4m"
```

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

#### four-colors-incompatible-stride.y4m
A 962x540 raw YUV single frame video with 4 color blocks (Y,R,G,B) and a GL
incompatible stride. Converted from four-colors.mp4 using ffmpeg:
```
ffmpeg -i four-colors.mp4 -vf "scale=w=962:h=540,format=yuv420p" -frames:v 1 four-colors-incompatible-stride.y4m
```

#### four-colors-vp9.webm
A 960x540 vp9 video with 4 color blocks (Y,R,G,B) in every frame. This is
converted from four-colors.mp4 by ffmpeg.

#### four-colors-vp9-i420a.webm
A 960x540 yuva420p vp9 video with 4 color blocks (Y,R,G,B) in every frame. This
is converted from four-colors.mp4 by adding an opacity of 0.5 using ffmpeg.


#### four-colors-av1.mp4
```
ffmpeg -y -i four-colors.png -t 2 \
       -vf "colorspace=bt709:iall=bt709:fast=1,scale=960:540,setsar=1:1" \
       -c:v libaom-av1 -b:v 2M \
       -pix_fmt yuv420p -movflags +write_colr four-colors-av1.mp4
```

#### four-colors-hevc.mp4
```
ffmpeg -y -i four-colors.png -t 2 \
       -vf "colorspace=bt709:iall=bt709:fast=1,scale=960:540,setsar=1:1" \
       -c:v libx265 -crf 26 \
       -pix_fmt yuv420p -movflags +write_colr four-colors-hevc.mp4
```

#### bear-320x180-hi10p.mp4
#### bear-320x240-vp9_profile2.webm
VP9 encoded video with profile 2 (10-bit, 4:2:0).
Codec string: vp09.02.10.10.01.02.02.02.00.

#### vp9-hdr-init-segment.mp4
Init segment for a VP9.2 HDR in MP4 file; from https://crbug.com/1102200#c6. The
SmDm and CoLL boxes have been added using mp4edit:

mp4edit.exe --insert moov/trak/mdia/minf/stbl/stsd/vp09:smdm.bin \
            --insert moov/trak/mdia/minf/stbl/stsd/vp09:coll.bin \
            vp9-hdr-init-segment.mp4 fixed.mp4

smdm.bin and coll.bin generated with program from https://crbug.com/1123430#c5.

#### bear-320x180-10bit-frame-\{0,1,2,3\}.h264
The first four frames of the H.264 version of bear-av1-320x180-10bit.mp4 created
using the following command.
`ffmpeg -i bear-av1-320x180-10bit.mp4 -vcodec h264 -vframes 4 bear-320x180-10bit-4frames.h264`
The file is then split into bitstreams each of which contains a single frame, so
that they contain frames as below.
bear-320x180-10bit-frame-0.h264: SPS+PPS+Single IDR
bear-320x180-10bit-frame-1.h264: B
bear-320x180-10bit-frame-2.h264: B
bear-320x180-10bit-frame-3.h264: P

#### gbrp.png

A screenshot frame captured from `gbrp-av1.mp4` on macOS 14.

#### gbrp-h264.mp4

H.264 encoded video with GBR colorspace matrix and 4:4:4 chroma sampling.
```
ffmpeg -f lavfi -i testsrc=s=320x240:r=1:d=1 -pix_fmt gbrp -color_range 2 -colorspace 0 -color_primaries 1 -color_trc 13 -c:v libx264rgb gbrp-h264.mp4
```

#### gbrp-h265.mp4

H.265 encoded video with GBR colorspace matrix and 4:4:4 chroma sampling.
```
ffmpeg -f lavfi -i testsrc=s=320x240:r=1:d=1 -pix_fmt gbrp -color_range 2 -colorspace 0 -color_primaries 1 -color_trc 13 -c:v libx265 gbrp-h265.mp4
```

#### gbrp-vp9.mp4

VP9 encoded video with GBR colorspace matrix and 4:4:4 chroma sampling.
```
ffmpeg -f lavfi -i testsrc=s=320x240:r=1:d=1 -pix_fmt gbrp -color_range 2 -colorspace 0 -color_primaries 1 -color_trc 13 -c:v vp9 gbrp-vp9.mp4
```

#### gbrp-av1.mp4

AV1 encoded video with GBR colorspace matrix and 4:4:4 chroma sampling.
```
ffmpeg -f lavfi -i testsrc=s=320x240:r=1:d=1 -pix_fmt gbrp -color_range 2 -colorspace 0 -color_primaries 1 -color_trc 13 -c:v av1 gbrp-av1.mp4
```

#### ebu-3213-e-vp9.mp4

VP9 encoded video with `EBU_3213_E` colorspace primary.

NOTE: FFmpeg can't convert a video to `EBU_3213_E` primary directly, the
workaround is to convert it into `BT470BG` primary first because they should
be identical, and then tag the primary as `EBU_3213_E` inside the container.
```
ffmpeg -f lavfi -i testsrc=s=320x240:r=1:d=1 -pix_fmt yuv420p -vf "colorspace=space=bt470bg:range=tv:primaries=smpte170m:trc=smpte170m:ispace=smpte170m:irange=tv:iprimaries=smpte170m:itrc=smpte170m" -color_range 1 -colorspace 6 -color_primaries 22 -color_trc 6 -c:v vp9 ebu-3213-e-vp9.mp4
```

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

#### av1-I-frame-320x240
vpxdec media/test/data/bear-vp9.webm -o bear.y4m
aomenc -o bear.ivf -p 2 --target-bitrate=150 bear.y4m --limit=1 --ivf
tail -c +45 bear.ivf > av1-I-frame-320x240

#### av1-I-frame-1280x720
Same as av1-I-frame-320x240 but using bear-1280x720.webm as input.

#### av1-monochrome-I-frame-320x240-[8,10,12]bpp
Same as av1-I-frame-320x240 with --monochrome and -b=[8,10,12] aomenc options.

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

#### bear-1280x720-hls-clear-mpl.m3u8
A single-segment hls media playlist which plays bear-1280x720-hls.ts.

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

#### HLS - directory
Samples of assorted playlist types and a README file explaining how each sample
is generated.

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

Additional containers created by Dolby:

* ac4-ajoc.ac4                 -- encoded with bitstream version 2, presentation version 1 and prosentation level 3
* ac4-channel-based-coding.ac4 -- encoded with bitstream version 2, presentation version 1 and prosentation level 1
* ac4-ims.ac4                  -- encoded with bitstream version 2, presentation version 2

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

#### test-25fps.av1.ivf:
The av1 video whose content is the same as test-25fps.h264.
```
ffmpeg -i test-25fps.h264 -vcodec libaom-av1 test-25fps.av1.ivf
```

#### test-25fps.av1.ivf.json:
JSON file that contains all metadata related to test-25fps.av1.ivf, used by the
video\_decode\_accelerator\_tests. This includes the video codec, resolution and
md5 checksums of individual video frames when converted to the I420 format.

#### test-25fps.hevc:
H.265/HEVC video whose content is the same as test-25fps.h264.
```
ffmpeg -i test-25fps.h264 -vcodec hevc test25fps.hevc
```

#### test-25fps.hevc.json:
JSON file that contains all metadata related to test-25fps.hevc, used by the
video\_decode\_accelerator\_tests. This includes the video codec, resolution and
md5 checksums of individual video frames when converted to the I420 format.

#### test-25fps.hevc10:
10-bit H.265/HEVC video whose content is the same as test-25fps.h264 but
converted to 10bpp.
```
ffmpeg -i test-25fps.h264 -vcodec hevc -pix_fmt yuv420p10le test25fps.hevc10
```

#### test-25fps.hevc10.json:
JSON file that contains all metadata related to test-25fps.hevc10, used by the
video\_decode\_accelerator\_tests. This includes the video codec, resolution and
md5 checksums of individual video frames when converted to the I420 format.

### VP9 video with raw vp9 frames

#### buck-1280x720-vp9.webm
1280x720 version of Big Buck Bunny https://peach.blender.org/ muxed with raw
vp9 frames (versus superframes).


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

### VEA test files:

#### bear_320x192_40frames.yuv.webm
First 40 raw i420 frames of bear-1280x720.mp4 scaled down to 320x192 for
video_encode_accelerator_tests. Encoded with vp9 lossless:
`ffmpeg -pix_fmt yuv420p -s:v 320x192 -r 30 -i bear_320x192_40frames.yuv -lossless 1 bear_320x192_40frames.yuv.webm`

#### bear_640x384_40frames.yuv.webm
First 40 raw i420 frames of bear-1280x720.mp4 scaled down to 340x384 for
video_encode_accelerator_tests. Encoded with vp9 lossless:
`ffmpeg -pix_fmt yuv420p -s:v 640x384 -r 30 -i bear_640x384_40frames.yuv -lossless 1 bear_640x384_40frames.yuv.webm`


### ImageProcessor Test Files

#### bear\_320x192.i420.yuv.webm
First frame of bear\_320x192\_40frames.yuv for image\_processor_test.
To get the uncompressed yuv, execute the following command.
`vpxdec bear_320x192.i420.yuv.webm --rawvideo -o bear_320x192.i420.yuv`

#### bear\_320x192.i420.yuv.json
Metadata describing bear\_320x192.i420.yuv.

#### bear\_320x192.nv12.yuv
First frame of bear\_320x192\_40frames.yuv for image\_processor_test and
formatted nv12.
To get the uncompressed yuv, execute the following command.
`ffmpeg -s:v 320x192 -pix_fmt yuv420p -i bear_320x192.i420.yuv  -c:v rawvideo -pix_fmt nv12 bear_320x192.nv12.yuv`

#### bear\_320x192.nv12.yuv.json
Metadata describing bear\_320x192.nv12.yuv.

#### bear\_320x192.yv12.yuv
First frame of bear\_320x192\_40frames.yv12.yuv for image\_processor_test and
formatted yv12.
To get the uncompressed yuv, execute the following command.
`ffmpeg -s:v 320x192 -pix_fmt yuv420p -i bear_320x192.i420.yuv  -c:v rawvideo -pix_fmt yuv420p -vf shuffleplanes=0:2:1 bear_320x192.yv12.yuv`

#### bear\_320x192.rgba
RAW RGBA format data. This data is created from bear\_320x192.i420.yuv.
Alpha channel is always 0xFF.
To get the uncompressed yuv, execute the following command.
`ffmpeg -s 320x192 -pix_fmt yuv420p -i bear_320x192.i420.yuv -vcodec rawvideo -f image2 -pix_fmt rgba bear_320x192.rgba`

#### bear\_320x192.bgra
RAW BGRA format data. This data is created from bear\_320x192.i420.yuv.
Alpha channel is always 0xFF.
To get the uncompressed yuv, execute the following command.
`ffmpeg -s 320x192 -pix_fmt yuv420p -i bear_320x192.i420.yuv -vcodec rawvideo -f image2 -pix_fmt bgra bear_320x192.bgra`

#### bear\_192x320\_90.nv12.yuv
Rotate bear\_320x192.nv12.yuv by 90 degrees clockwise.
`ffmpeg -s:v 320x192 -pix_fmt nv12 -i bear_320x192.nv12.yuv -vf transpose=1 -c:v rawvideo -pix_fmt nv12 bear_192x320_90.nv12.yuv`

#### bear\_192x320\_90.nv12.yuv.json
Metadata describing bear\_192x320\_90.nv12.yuv

#### bear\_320x192\_180.nv12.yuv
Rotate bear\_320x192.nv12.yuv by 180 degrees clockwise.
`ffmpeg -s:v 320x192 -pix_fmt nv12 -i bear_320x192.nv12.yuv -vf "transpose=2,transpose=2" -c:v rawvideo -pix_fmt nv12 bear_320x192_180.nv12.yuv`

#### bear\_320x192\_180.nv12.yuv.json
Metadata describing bear\_320x192\_180.nv12.yuv

#### bear\_192x320\_270.nv12.yuv
Rotate bear\_320x192.nv12.yuv by 270 degrees clockwise.
`ffmpeg -s:v 320x192 -pix_fmt nv12 -i bear_320x192.nv12.yuv -vf transpose=2 -c:v rawvideo -pix_fmt nv12 bear_192x320_270.nv12.yuv`

#### bear\_192x320\_270.nv12.yuv.json
Metadata describing bear\_192x320\_270.nv12.yuv

#### puppets-1280x720.nv12.yuv
RAW NV12 format data. The width and height are 1280 and 720, respectively.
This data is created from peach\_pi-1280x720.jpg by the following command.
`ffmpeg -i peach_pi-1280x720.jpg -s 1280x720 -pix_fmt nv12 puppets-1280x720.nv12.yuv`

To get the uncompressed yuv, execute the following commands.
`vpxdec puppets-1280x720.i420.yuv.webm --rawvideo -o puppets-1280x720.i420.yuv`
`ffmpeg -s:v 1280x720 -pix_fmt yuv420p -i puppets-1280x720.i420.yuv -c:v rawvideo -pix_fmt nv12 puppets-1280x720.nv12.yuv`

#### puppets-640x360.nv12.yuv
RAW NV12 format data. The width and height are 640 and 360, respectively.
To get the uncompressed yuv, execute the following command.
`ffmpeg -s:v 1280x720 -pix_fmt nv12 -i puppets-1280x720.nv12.yuv -vf scale=640x360 -c:v rawvideo -pix_fmt nv12 puppets-640x360.nv12.yuv`

#### puppets-480x270.nv12.yuv
RAW NV12 format data. The width and height are 640 and 360, respectively.
To get the uncompressed yuv, execute the following command.
`ffmpeg -s:v 1280x720 -pix_fmt nv12 -i puppets-1280x720.nv12.yuv -vf scale=480x270 -c:v rawvideo -pix_fmt nv12 puppets-480x270.nv12.yuv`

### puppets-640x360\_in\_640x480.nv12.yuv
RAW NV12 format data. The width and height are 640 and 480, respectively.
The meaningful image is at the rectangle (0, 0, 640x360) in the image. The area
outside this rectangle is black.
To get the uncompressed yuv, execute the following command.
`ffmpeg -s:v 640x360 -pix_fmt nv12 -i puppets-640x360.nv12.yuv -vf "scale=640:480:force_original_aspect_ratio=decrease,pad=640:480:(ow-iw)/2:(oh-ih)/2" -c:v rawvideo -pix_fmt nv12 puppets-640x360_in_640x480.nv12.yuv`

#### puppets-320x180.nv12.yuv
RAW NV12 format data. The width and height are 320 and 180, respectively.
To get the uncompressed yuv, execute the following command.
`ffmpeg -s:v 1280x720 -pix_fmt nv12 -i puppets-1280x720.nv12.yuv -vf scale=320x180 -c:v rawvideo -pix_fmt nv12 puppets-320x180.nv12.yuv`

###  VP9 parser test files:

#### bear-vp9.ivf
Created using "avconv -i bear-vp9.webm -vcodec copy -an -f ivf bear-vp9.ivf".

#### bear-vp9.ivf.context

#### test-25fps.vp9.context
Manually dumped from libvpx with bear-vp9.ivf and test-25fps.vp9. See
vp9_parser_unittest.cc for description of their format.


### H264 decoder test files:

#### blackwhite\_yuv444p-frame.h264
The first frame of blackwhite_yuv444p.mp4 by the following command.
`ffmpeg -i blackwhite_yuv444p.mp4 -vcodec copy -vframes 1 blackwhite_yuv444p-frame.h264`

### HEVC parser/decoder test files:

#### bear.hevc
Used by h265_parser_unittest.cc.

#### bbb.hevc
Used by h265_parser_unittest.cc. Copied from bbb_hevc_176x144_176kbps_60fps.hevc
in Android repo.

#### bear-sps-pps.hevc
SPS and PPS from bear.hevc for h265_decoder_unittest.cc.

#### bear-frame\{0,1,2,3,4,5\}.hevc
Single IDR, P, B, B, B, P frames respectively from bear.hevc for
h265_decoder_unittest.cc.

#### bear-320x180-10bit-frame-\{0,1,2,3\}.hevc
The first four frames of the HEVC version of bear-av1-320x180-10bit.mp4 created
using the following command.
`ffmpeg -i bear-av1-320x180-10bit.mp4 -vcodec hevc -vframes 4 bear-320x180-10bit-4frames.hevc`
The file is then split into bitstreams each of which contains a single frame, so
that they contain frames as below.
bear-320x180-10bit-frame-0.hevc: SPS+PPS+Single IDR
bear-320x180-10bit-frame-1.hevc: B
bear-320x180-10bit-frame-2.hevc: B
bear-320x180-10bit-frame-3.hevc: P

#### bear-1280x720-hevc-10bit-hdr10.hevc
AnnexB version of bear-1280x720-hevc-10bit-hdr10.mp4 created using the following command:
`ffmpeg -i bear-1280x720-hevc-10bit-hdr10.mp4 -c:v copy -bsf hevc_mp4toannexb bear-1280x720-hevc-10bit-hdr10.hevc',
used by h265_parser_unittest.cc.

#### blackwhite\_yuv444p-frame.hevc
The first frame of blackwhite_yuv444p.mp4 coded in HEVC by the following command.
`ffmpeg -i blackwhite_yuv444p.mp4 -vcodec hevc -vframes 1 blackwhite_yuv444p-frame.hevc`

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
AC3 audio in framented MP4, generated with bento4 by the following command:
```
mp4mux --track bear.ac3 bear-ac3-only.mp4
mp4fragment bear-ac3-only.mp4 bear-ac3-only-frag.mp4
```

#### bear-eac3-only-frag.mp4
EAC3 audio in framented MP4, generated with bento4 by the following command:
```
mp4mux --track bear.eac3 bear-eac3-only.mp4
mp4fragment bear-eac3-only.mp4 bear-eac3-only-frag.mp4
```

#### ac4-only-ajoc-frag.mp4
AC4 A-JOC audio in framented MP4, generated with bento4 by the following command:
```
mp4mux --track ac4-ajoc.ac4 ac4-only-ajoc.mp4
mp4fragment ac4-only-ajoc.mp4 ac4-only-ajoc-frag.mp4
```

#### ac4-only-channel-based-coding-frag.mp4
AC4 channel based audio in framented MP4, generated with bento4 by the following command:
```
mp4mux --track ac4-channel-based-coding.ac4 ac4-only-channel-based-coding.mp4
mp4fragment ac4-only-channel-based-coding.mp4 ac4-only-channel-based-coding-frag.mp4
```

#### ac4-only-ims-frag.mp4
AC4 immersive stereo(IMS) audio in framented MP4, generated with bento4 by the following command:
```
mp4mux --track ac4-ims.ac4 ac4-only-ims.mp4
mp4fragment ac4-only-ims.mp4 ac4-only-ims-frag.mp4
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

#### bear-1280x720-hevc.mp4
HEVC video stream with 8-bit main profile, generated with
```
ffmpeg -i bear-1280x720.mp4 -vcodec hevc bear-1280x720-hevc.mp4
```

#### bear-1280x720-hevc-no-audio.mp4
HEVC video stream with 8-bit main profile, generated with
```
ffmpeg -i bear-1280x720-hevc.mp4 -vcodec copy -an bear-1280x720-hevc-no-audio.mp4
```

#### bear-1280x720-hevc-10bit.mp4
HEVC video stream with 10-bit main10 profile, generated with
```
ffmpeg -i bear-1280x720.mp4 -vcodec hevc -pix_fmt yuv420p10le bear-1280x720-hevc-10bit.mp4
```
#### bear-1280x720-hevc-10bit-no-audio.mp4
HEVC video stream with 10-bit main10 profile, generated with
```
ffmpeg -i bear-1280x720-hevc-10bit.mp4 -vcodec copy -an bear-1280x720-hevc-10bit-no-audio.mp4
```

#### bear-1280x720-hevc-8bit-422.mp4
HEVC video stream with 8-bit 422 range extension profile, generated with
```
ffmpeg -i bear-1280x720.mp4 -vcodec hevc -pix_fmt yuv422p bear-1280x720-hevc-8bit-422.mp4
```

#### bear-1280x720-hevc-8bit-422-no-audio.mp4
HEVC video stream with 8-bit 422 range extension profile, generated with
```
ffmpeg -i bear-1280x720-hevc-8bit-422.mp4 -vcodec copy -an bear-1280x720-hevc-8bit-422-no-audio.mp4
```

#### bear-1280x720-hevc-8bit-444-no-audio.mp4
HEVC video stream with 8-bit 444 range extension profile, generated with
```
ffmpeg -i bear-1280x720.mp4 -vcodec hevc -an -pix_fmt yuv444p bear-1280x720-hevc-8bit-444-no-audio.mp4
```

#### bear-1280x720-hevc-10bit-422.mp4
HEVC video stream with 10-bit 422 range extension profile, generated with
```
ffmpeg -i bear-1280x720.mp4 -vcodec libx265 -pix_fmt yuv422p10le bear-1280x720-hevc-10bit-422.mp4
```

#### bear-1280x720-hevc-10bit-422-no-audio.mp4
HEVC video stream with 10-bit 422 range extension profile, generated with
```
ffmpeg -i bear-1280x720-hevc-10bit-422.mp4 -vcodec copy -an bear-1280x720-hevc-10bit-422-no-audio.mp4
```

#### bear-1280x720-hevc-10bit-444.mp4
HEVC video stream with 10-bit 444 range extension profile, generated with
```
ffmpeg -i bear-1280x720.mp4 -vcodec libx265 -pix_fmt yuv444p10le bear-1280x720-hevc-10bit-444.mp4
```

#### bear-1280x720-hevc-10bit-444-no-audio.mp4
HEVC video stream with 10-bit 444 range extension profile, generated with
```
ffmpeg -i bear-1280x720-hevc-10bit-444.mp4 -vcodec copy -an bear-1280x720-hevc-10bit-444-no-audio.mp4
```

#### bear-1280x720-hevc-12bit-420.mp4
HEVC video stream with 12-bit 420 range extension profile, generated with
```
ffmpeg -i bear-1280x720.mp4 -vcodec libx265 -pix_fmt yuv420p12le bear-1280x720-hevc-12bit-420.mp4
```

#### bear-1280x720-hevc-12bit-422.mp4
HEVC video stream with 12-bit 422 range extension profile, generated with
```
ffmpeg -i bear-1280x720.mp4 -vcodec libx265 -pix_fmt yuv422p12le bear-1280x720-hevc-12bit-422.mp4
```

#### bear-1280x720-hevc-12bit-444.mp4
HEVC video stream with 12-bit 444 range extension profile, generated with
```
ffmpeg -i bear-1280x720.mp4 -vcodec libx265 -pix_fmt yuv444p12le bear-1280x720-hevc-12bit-444.mp4
```

#### bear-1280x720-hevc-10bit-hdr10.mp4
HEVC video stream with HDR10 metadata included, generated with
````
ffmpeg -i bear-1280x720.mp4 -vcodec libx265 -x265-params colorprim=bt2020:transfer=smpte2084:colormatrix=bt2020nc:master-display="G(13250,34500)B(7500,3000)R(34000,16000)WP(15635,16450)L(10000000,500)":max-cll=1000,400 -pix_fmt yuv420p10le bear-1280x720-hevc-10bit-hdr10.mp4 // nocheck
````

#### bear-3840x2160-hevc.mp4
HEVC video stream with 8-bit main profile, generated with
```
ffmpeg -i bear-1280x720.mp4 -vf "scale=3840:2160,setpts=4*PTS" -c:v libx265 -crf 28 -c:a copy bear-3840x2160-hevc.mp4
```

### MP4 file with Dolby Vision

#### glass-blowing2-dolby-vision-profile-5-frag.mp4
Original sample from `https://media.developer.dolby.com/DolbyVision_Atmos/mp4/iOS_P5_GlassBlowing2_1920x1080%4059.94fps_15200kbps.mp4`. Dolby Vision profile 5 video stream generated using FFmpeg/mp4mux/mp4fragment with the following commands:
```
ffmpeg -ss 0:00:11 -i iOS_P5_GlassBlowing2_1920x1080@59.94fps_15200kbps.mp4 -t 1 -vcodec copy -an glass-blowing2-dolby-vision-profile-5.hevc
mp4mux --track h265:glass-blowing2-dolby-vision-profile-5.hevc#dv_profile=5,dv_bc=0,format="dvh1",frame_rate=60,video glass-blowing2-dolby-vision-profile-5.mp4
mp4fragment glass-blowing2-dolby-vision-profile-5.mp4 glass-blowing2-dolby-vision-profile-5-frag.mp4
```

#### glass-blowing2-dolby-vision-profile-8-1-frag.mp4
Original sample from `https://media.developer.dolby.com/DolbyVision_Atmos/mp4/P81_GlassBlowing2_1920x1080%4059.94fps_15200kbps_fmp4.mp4`. Dolby Vision profile 8.1 video stream generated using FFmpeg/mp4mux/mp4fragment with the following commands:
```
ffmpeg -ss 0:00:11 -i P81_GlassBlowing2_1920x1080@59.94fps_15200kbps_fmp4.mp4 -t 1 -vcodec copy -an glass-blowing2-dolby-vision-profile-8-1.hevc
mp4mux --track h265:glass-blowing2-dolby-vision-profile-8-1.hevc#dv_profile=8,dv_bc=1,format="hvc1",frame_rate=60,video glass-blowing2-dolby-vision-profile-8-1.mp4
mp4fragment glass-blowing2-dolby-vision-profile-8-1.mp4 glass-blowing2-dolby-vision-profile-8-1-frag.mp4
```

#### color_pattern_24_dvhe05_1920x1080__dvh1_st-3sec-frag-cenc.mp4
Original sample from `https://crbug.com/363270181#comment6`. Dolby Vision
profile 5 video stream encrypted using [Shaka Packager] with the following
commands:
```
ffmpeg -ss 0:00:11 -i color_pattern_24_dvhe05_1920x1080__dvh1_st.mp4 -t 3 -vcodec copy -an color_pattern_24_dvhe05_1920x1080__dvh1_st.hevc
mp4mux --track h265:color_pattern_24_dvhe05_1920x1080__dvh1_st.hevc#dv_profile=5,dv_bc=0,format="dvh1",frame_rate=24,video color_pattern_24_dvhe05_1920x1080__dvh1_st-3sec.mp4
mp4fragment color_pattern_24_dvhe05_1920x1080__dvh1_st-3sec.mp4 color_pattern_24_dvhe05_1920x1080__dvh1_st-3sec-frag.mp4

packager in=color_pattern_24_dvhe05_1920x1080__dvh1_st-3sec-frag.mp4,stream=video,output=color_pattern_24_dvhe05_1920x1080__dvh1_st-3sec-frag-cenc.mp4 \
         --enable_raw_key_encryption \
         --protection_scheme cenc \
         --segment_duration 0.5 \
         --clear_lead 0 \
         --keys label=:key_id=30313233343536373839303132333435:key=ebdd62f16814d27b68ef122afce4ae3c \
         --pssh 000000327073736800000000EDEF8BA979D64ACEA3C827DCD51D21ED000000121210303132333435363738393031323334350000003470737368010000001077EFECC0B24D02ACE33C1E52E2FB4B000000013031323334353637383930313233343500000000
```

#### color_pattern_24_dvhe05_1920x1080__dvh1_st-3sec-frag-cenc-clearlead-2sec.mp4
Dolby Vision profile 5 video stream with clear lead generated using
[Shaka Packager] with the following commands:
```
packager in=color_pattern_24_dvhe05_1920x1080__dvh1_st-3sec-frag.mp4,stream=video,output=color_pattern_24_dvhe05_1920x1080__dvh1_st-3sec-frag-cenc-clearlead-2sec.mp4 \
         --enable_raw_key_encryption \
         --protection_scheme cenc \
         --segment_duration 0.5 \
         --clear_lead 2 \
         --keys label=:key_id=30313233343536373839303132333435:key=ebdd62f16814d27b68ef122afce4ae3c \
         --pssh 000000327073736800000000EDEF8BA979D64ACEA3C827DCD51D21ED000000121210303132333435363738393031323334350000003470737368010000001077EFECC0B24D02ACE33C1E52E2FB4B00000001303132333435363738393031323334350000000
```

#### color_pattern_24_dvhe081_compressed_rpu_1920x1080__dvh1_st-3sec-frag-cenc.mp4
Original sample from `https://crbug.com/363270181#comment6`. Dolby Vision
profile 8.1 video stream encrypted using [Shaka Packager] with the following
commands:
```
ffmpeg -ss 0:00:11 -i color_pattern_24_dvhe081_compressed_rpu_1920x1080__dvh1_st.mp4 -t 3 -vcodec copy -an color_pattern_24_dvhe081_compressed_rpu_1920x1080__dvh1_st.hevc
mp4mux --track h265:color_pattern_24_dvhe081_compressed_rpu_1920x1080__dvh1_st.hevc#dv_profile=5,dv_bc=0,format="dvh1",frame_rate=24,video color_pattern_24_dvhe081_compressed_rpu_1920x1080__dvh1_st-3sec.mp4
mp4fragment color_pattern_24_dvhe081_compressed_rpu_1920x1080__dvh1_st-3sec.mp4 color_pattern_24_dvhe081_compressed_rpu_1920x1080__dvh1_st-3sec-frag.mp4

packager in=color_pattern_24_dvhe081_compressed_rpu_1920x1080__dvh1_st-3sec-frag.mp4,stream=video,output=color_pattern_24_dvhe081_compressed_rpu_1920x1080__dvh1_st-3sec-frag-cenc.mp4 \
         --enable_raw_key_encryption \
         --protection_scheme cenc \
         --segment_duration 0.5 \
         --clear_lead 0 \
         --keys label=:key_id=30313233343536373839303132333435:key=ebdd62f16814d27b68ef122afce4ae3c \
         --pssh 000000327073736800000000EDEF8BA979D64ACEA3C827DCD51D21ED000000121210303132333435363738393031323334350000003470737368010000001077EFECC0B24D02ACE33C1E52E2FB4B000000013031323334353637383930313233343500000000
```

#### color_pattern_24_dvhe081_compressed_rpu_1920x1080__dvh1_st-3sec-frag-cenc-clearlead-2sec.mp4
Dolby Vision profile 8.1 video stream with clear lead generated using
[Shaka Packager] with the following commands:
```
packager in=color_pattern_24_dvhe081_compressed_rpu_1920x1080__dvh1_st-3sec-frag.mp4,stream=video,output=color_pattern_24_dvhe081_compressed_rpu_1920x1080__dvh1_st-3sec-frag-cenc-clearlead-2sec.mp4 \
         --enable_raw_key_encryption \
         --protection_scheme cenc \
         --segment_duration 0.5 \
         --clear_lead 2 \
         --keys label=:key_id=30313233343536373839303132333435:key=ebdd62f16814d27b68ef122afce4ae3c \
         --pssh 000000327073736800000000EDEF8BA979D64ACEA3C827DCD51D21ED000000121210303132333435363738393031323334350000003470737368010000001077EFECC0B24D02ACE33C1E52E2FB4B000000013031323334353637383930313233343500000000
```

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

#### multitrack-disabled.mp4
H.264 video stream with the first track marked as disabled, generated with
````
ffmpeg -f lavfi -i "color=c=white:d=1" -f lavfi -i "testsrc2=d=1" -map 0 -disposition:v:0 0 -map 1 -disposition:v:1 default -c:v libx264 multitrack-disabled.mp4
````

#### track-disabled.mp4
H.264 video stream with the only track disabled, generated with
````
ffmpeg -f lavfi -i "color=c=white:d=1" -map 0 -disposition:v:0 0 -c:v libx264 track-disabled.mp4
````

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

### Spherical metadata WebM files

#### bear-spherical-metadata.webm
bear_silent.webm video injected with "stereo_mode=SIDE_BY_SIDE_LEFT_EYE_FIRST", "projectionType=EQUIRECTANGULAR",
and projection pose_yaw, pose_pitch, and pose_roll = 10, 20, and 30 respectively.

### Opus pre-skip and end-trimming test clips
https://people.xiph.org/~greg/opus_testvectors/

* opus-trimming-test.mp4
* opus-trimming-test.ogg
* opus-trimming-test.webm

[libaom test vectors]: https://aomedia.googlesource.com/aom/+/master/test/test_vectors.cc
[libaom LICENSE]: https://source.chromium.org/chromium/chromium/src/+/main:media/test/data/licenses/AOM-LICENSE


### DTS Audio

#### dts.bin
A single DTS Coherent Acoustics audio frame

#### dtsx.bin
A single DTS:X P2 Coherent Acoustics audio frame

#### bear_dtsc.mp4
Moov box with track of DTS Coherent Acoustics Audio, no mdat box.

Generated using the following commands:
```
# start with ~1 second long PCM 2 channel audio and extend to three seconds
ffmpeg -i bear_pcm.wav -y bear.ts
ffmpeg -i "concat:bear.ts|bear.ts|bear.ts" -c copy bear2.wav
# make a 6 channel WAV
ffmpeg -i bear2.wav -i bear2.wav -i bear2.wav -ar 48000 -filter_complex 'amerge=inputs=3' -y bear6.wav
ffmpeg -i bear6.wav -c:a dtsS -movflags frag_keyframe -y bear_dtsc.mp4
# truncate to size of moov box (truncate -s)
```

#### bear_dtse.mp4
Moov box with track of DTS Express Audio, no mdat box.

Generated using the following commands:
```
# start with ~1 second long PCM 2 channel audio and extend to three seconds
ffmpeg -i bear_pcm.wav -y bear.ts
ffmpeg -i "concat:bear.ts|bear.ts|bear.ts" -c copy bear2.wav
# make a 6 channel WAV
ffmpeg -i bear2.wav -i bear2.wav -i bear2.wav -ar 48000 -filter_complex 'amerge=inputs=3' -y bear6.wav
# create DTS CA, DTS Express, DTS:X P2 mp4 files
ffmpeg -i bear6.wav -c:a dtsS -b:a 255000 -movflags frag_keyframe -y bear_dtse.mp4
# truncate to size of moov box (truncate -s)
```

#### bear_dtsx.mp4
Moov box with track of DTS:X Profile 2 Audio, no mdat box.

Generated using the following commands:
```
# start with ~1 second long PCM 2 channel audio and extend to three seconds
ffmpeg -i bear_pcm.wav -y bear.ts
ffmpeg -i "concat:bear.ts|bear.ts|bear.ts" -c copy bear2.wav
# make a 6 channel WAV
ffmpeg -i bear2.wav -i bear2.wav -i bear2.wav -ar 48000 -filter_complex 'amerge=inputs=3' -y bear6.wav
# create DTS CA, DTS Express, DTS:X P2 mp4 files
ffmpeg -i bear6.wav -c:a dtsxS -b:a 160000 -movflags frag_keyframe -y bear_dtsx.mp4
# truncate to size of moov box (truncate -s)
```
### one_frame_1280x720.mjpeg
It's a single frame mjpeg data. Resolution: 1280x720, color primary: sRGB, transfer function: BT.709, color matrix: BT.601, color range: full-range.

### avc-bitstream-format-0.h264
The first 2 frames of the H.264 with bitstream format (NALU length)

avc-bitstream-format-0.h264: IDR
- ffmpeg -y -i bear-1280x720.mp4 -vcodec copy -f m4v avc-bitstream-format-0.h264
avc-bitstream-format-1.h264: Non-IDR
- split bear-1280x720.mp4 to annexb files by command of
  ffmpeg -i %1 -f image2 -vcodec copy -bsf h264_mp4toannexb "%d.h264"
- manually convert one of created Non-IDR annexb file to avc bitstream.
  (replace annexb start code with length)

### reference-frame-scaling-test.ivf
Video stream for testing reference frame scaling in AV1 files where resolution changes at various stages in a AV1 video stream.
- 300 frames.
- First 100 frames Resolution: 1920 x 1080.
- Next 100 frames Resolution: 1280 x 720.
- Last 100 frames Resolution: 960 x 540.

### hls/ directory
This directory contains all the HLS files needed to run pipeline integration tests against the HLS demuxer. The readme file in this directory contains specific steps to regenerate media and manifest files.
