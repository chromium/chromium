# VA-API fake backend for libva

VA-API is an API for video/image decoding/encoding acceleration implemented by
[libva]. The files in this folder provide a fake backend for it
intended to be used in tests.

It can be explicitly exercised by running e.g.:

    LIBVA_DRIVER_NAME="libfake" vainfo

wherever it might be installed. See https://tinyurl.com/libva-fake-driver for
its original implementation. This document is outdated but still provides the
general idea for this directory.

[libva]: https://github.com/intel/libva