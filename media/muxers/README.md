# //media/muxers

This directory contains code for muxing mkv (webm) and mp4 files.
 * https://en.wikipedia.org/wiki/Matroska
 * https://en.wikipedia.org/wiki/WebM
 * https://en.wikipedia.org/wiki/MP4_file_format

Muxers are based off the `media::Muxer` interface. These muxers are
primarily used by MediaRecorder and ChromeOS video recording.

Since audio and video streams often come from sources with different
clocks, timestamp alignment can be handled by `MuxerTimestampAdapter`.
More details on synchronization can be found in the
[Media Capture and Streams](https://www.w3.org/TR/mediacapture-streams/#introduction)
spec.

Currently the following codecs are supported in each muxer:
 * mkv / webm: VP8, VP9, AV1, H.264 for video plus Opus for audio.
 * mp4: VP9, AV1, H.264 for video and Opus, AAC for audio.

Note: webm is a subset of mkv that technically only supports vp8, vp9,
and av1 codecs.
