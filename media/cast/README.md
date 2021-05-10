# media/cast/

This directory contains a collection of components related to streaming using
the Cast Streaming Protocol (over UDP network sockets). They encode/decode raw
audio or video frames, and send/receive encoded data reliably over a local area
network.

NOTE: This implementation is **deprecated**, and to be replaced soon by the one
found in `../../third_party/openscreen/src/cast/streaming/`. Contact
jophba@chromium.org for details.

# Directory Breakdown

* common/ - Collection of shared utility code and constants.

* logging/ - Packet/Frame logging, for study/experimentation of the protocol at
  runtime.

* net/ - Wire-level packetization and pacing.

* sender/ - Encoder front-ends and frame-level sender implementation for
  audio/video.

* test/ - A collection of end-to-end tests, experiments, benchmarks, and related
  utility code.

* test/receiver/ - A minimal receiver implementation, used only for end-to-end
  testing.
