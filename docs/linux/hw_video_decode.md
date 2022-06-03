# Enabling hardware <video> decode codepaths on linux

Hardware acceleration of video decode on Linux is
[unsupported](https://crbug.com/137247) in Chrome for user-facing builds. During
development (targeting other platforms) it can be useful to be able to trigger
the code-paths used on HW-accelerated platforms (such as CrOS and win7) in a
linux-based development environment. Here's one way to do so, with details based
on a gprecise setup.

*   Install pre-requisites: On Ubuntu Precise, at least, this includes:

    ```shell
    sudo apt-get install libtool libvdpau1 libvdpau-dev
    ```

*   Install and configure [libva](http://cgit.freedesktop.org/libva/)

    ```shell
    DEST=${HOME}/apps/libva
    cd /tmp
    git clone git://anongit.freedesktop.org/libva
    cd libva
    git reset --hard libva-1.2.1
    ./autogen.sh && ./configure --prefix=${DEST}
    make -j32 && make install
    ```

*   Install and configure the
    [VDPAU](http://cgit.freedesktop.org/vaapi/vdpau-driver) VAAPI driver

    ```shell
    DEST=${HOME}/apps/libva
    cd /tmp
    git clone git://anongit.freedesktop.org/vaapi/vdpau-driver
    cd vdpau-driver
    export PKG_CONFIG_PATH=${DEST}/lib/pkgconfig/:$PKG_CONFIG_PATH
    export LIBVA_DRIVERS_PATH=${DEST}/lib/dri
    export LIBVA_X11_DEPS_CFLAGS=-I${DEST}/include
    export LIBVA_X11_DEPS_LIBS=-L${DEST}/lib
    export LIBVA_DEPS_CFLAGS=-I${DEST}/include
    export LIBVA_DEPS_LIBS=-L${DEST}/lib
    make distclean
    unset CC CXX
    ./autogen.sh && ./configure --prefix=${DEST} --enable-debug
    find . -name Makefile |xargs sed -i 'sI/usr/lib/xorg/modules/driversI${DEST}/lib/driIg'
    sed -i -e 's/_(\(VAEncH264VUIBufferType\|VAEncH264SEIBufferType\));//' src/vdpau_dump.c
    make -j32 && rm -f ${DEST}/lib/dri/{nvidia_drv_video.so,s3g_drv_video.so} && make install
    ```

*   Add to args.gn:
    *   `target_os = "chromeos"` to link in `VaapiVideoDecodeAccelerator`
    *   `proprietary_codecs = true` and `ffmpeg_branding = "Chrome"` to
        allow Chrome to play h.264 content, which is the only codec
        VAVDA knows about today.
*   Rebuild chrome
*   Run chrome with `LD_LIBRARY_PATH=${HOME}/apps/libva/lib` in the environment,
    and with the `--no-sandbox` command line flag.
*   If things don't work, a Debug build (to include D\*LOG's) with
    `--vmodule=*content/common/gpu/media/*=10,gpu_video*=1` might be
    enlightening.

** note
NOTE THIS IS AN UNSUPPORTED CONFIGURATION AND LIKELY TO BE BROKEN AT ANY
POINT IN TIME
**

This page is purely here to help developers targeting supported HW `<video>`
decode platforms be more effective. Do not expect help if this setup fails to
work.
