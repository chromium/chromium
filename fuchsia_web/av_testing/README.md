## How to run the tests locally

When running locally, a local http server is needed to host the video files to
avoid the accessibility issue. A simple solution is to get a random vp8 file,
rename it to sample.webm, place it here. Note, if the build does not have
src-internal, it won't support video formats like h264.

By default, the script running locally expects the host machine is at
172.16.243.1.
