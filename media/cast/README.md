# //media/cast

This directory contains a collection of components related to streaming using
the Cast Streaming Protocol (over UDP network sockets). They encode/decode raw
audio or video frames, and send/receive encoded data reliably over a local area
network.

The Chromium project uses libcast, which is part of the
[Open Screen](https://chromium.googlesource.com/openscreen/) project, for
handling Cast Streaming. This implementation sits essentially as a translation
layer on top of libcast, providing things like encoders (which libcast, very
intentionally, tries to not be aware of) and Cast objects that are (hopefully)
easy to reason with.

The majority of the documentation for Cast Streaming can be found in libcast's
[documents folder](https://chromium.googlesource.com/openscreen/+/HEAD/cast/docs/).

## Directory Breakdown

* common/ - Collection of shared utility code and constants.

* encoding/ - Audio and video encoders and supporting classes.

* logging/ - Packet/Frame logging, for study/experimentation of the protocol at
  runtime.

* openscreen/ - classes and free functions for interfacing with libcast, the
                Cast Streaming implementation.

* sender/ - Frame level senders and supporting classes.

* test/ - Contains fakes, mocks, and other test utility code.
