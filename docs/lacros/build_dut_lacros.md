# Build DUT Lacros

## System requirements
The same as [linux](../linux/build_instructions.md)

## Glossary
DUT = Device under testing. Chrome book to run development images, including
locally build ash-chrome and lacros-chrome.

## Hardware setup
Follow the usual Chromebook development setup:

1. Put the CrOS device into developer mode. See
[Debug Button Shortcuts](https://chromium.googlesource.com/chromiumos/docs/+/master/debug_buttons.md)

2. Flash device
Initially you need
[Create a bootable USB stick](https://chromium.googlesource.com/chromiumos/docs/+/HEAD/simple_chrome_workflow.md#create-a-bootable-usb-stick)

    Future flashes can use SSH. See
[cros flash](https://chromium.googlesource.com/chromiumos/docs/+/master/cros_flash.md)

For Googlers, please see [go/lacros-build-dut](http://go/lacros-build-dut) for
corp network or WFH network setup.

## Chromium environment setup
Follow [simple chrome workflow](https://chromium.googlesource.com/chromiumos/docs/+/HEAD/simple_chrome_workflow.md)

### Setting up output directories to avoid confusion

There are several different binaries that can be built from the same repository
using different configurations. To avoid confusion, we recommend using
explicitly named output directories. For example:

out_device_lacros: the directory that holds artifacts for lacros-chrome running
on DUT on amd64. (See below for other architectures).

out_$BOARD: the directory that holds artifacts for ash-chrome running on DUT.
See “Build ash-chrome” section below for how to set up this directory.

### Build ash-chrome (wayland server) for a DUT

If you need to make window manager changes, you can update the ash-chrome binary
without reflashing the whole device. This requires that the device already have
a test image.

Update .gclient to include the board SDK. See the simple chrome workflow.

Googlers: Use internal board name, like "eve"

Non-Googlers: Use amd64-generic

Specify your board for the following commands
```
% export BOARD=eve  # Googlers
% export BOARD=amd64-generic  # Non-Googlers
```
The gn args for amd64-generic would be:
```
% cat out_amd64-generic/Release/args.gn

import("//build/args/chromeos/amd64-generic.gni")
use_goma = true
```
Build with:
```
% autoninja -C out_${BOARD}/Release/ chrome
```
Deploy with deploy_chrome:
```
% ./third_party/chromite/bin/deploy_chrome --build-dir=out_${BOARD}/Release \
 --device=<ip>:<port> --board=${BOARD}
```

### Build Lacros
We support x86_64 devices. See Appendix for ARM notes. These instructions use
the newer gclient-based workflow that does not require a simplechrome
"cros chrome-sdk" shell.

1. Update .gclient file
Use target_os = ["chromeos"], "cros_boards": "amd64-generic" and
"checkout_lacros_sdk": True.
A typical .gclient looks like this for Googlers:
```
solutions = [
  {
    "name": "src",
    "url": "https://chromium.googlesource.com/chromium/src.git",
    "managed": False,
    "custom_deps": {},
    "custom_vars": {
        "checkout_src_internal": True,
        "cros_boards": "amd64-generic:eve",
        "checkout_lacros_sdk": True,
    },
  },
]
target_os=["chromeos"]
```
eve is the DUT board at hand for local debugging.

For non-Googlers, remove the 'checkout_src_internal' and ':eve'.

Use "cros_boards": "arm-generic" for building Lacros on ARM.

Use “cros_boards”: “arm64-generic” for building Lacros on ARM64.

Multiple board names can be listed in “cros_boards” by using ‘:’ as a separator.
Double check the modifications to .gclient. Make sure that variables were added
under "custom_vars" -- failing to do so is a common mistake.

2. Run gclient sync

This will be slow the first time because it downloads the compiler toolchain.
Generate your args.gn, and then build Chrome.

3. Build Lacros
```
% mkdir -p out_device_lacros/Release
% echo 'import("//build/args/chromeos/amd64-generic-crostoolchain.gni")
target_os="chromeos"
is_chromeos_device=true
chromeos_is_browser_only=true
use_goma=true
is_chrome_branded=true
is_official_build=false
is_debug=false
use_thin_lto = false
is_cfi = false
is_component_build = false' > out_device_lacros/Release/args.gn
% gn gen out_device_lacros/Release
% autoninja -C out_device_lacros/Release chrome
```
Note: use arm-generic-crostoolchain.gni for 32-bit arm and
arm64-generic-crostoolchain.gni for 64-bit arm devices.

Note: Lacros build is per architecture. Thus, regardless of the board of your
testing devices, amd64-generic-crostoolchain.gni for x86_64 or
arm-generic-crostoolchain.gni for arm 32 bits and
arm64-generic-crostoolchain.gni should be imported.

#### Optimization level choices:
  1. is_debug=false
Release build. No DCHECKs. Runs fast.
  2. is_debug=false dcheck_always_on=true
Release build with DCHECKs. Has function symbols, but no locals. Some stack
frames may be missing from gdb and base::debug::StackTrace().Print(). Similar
to what runs in chromium CQ for linux-lacros-rel.
  3. is_debug=true is_component_build=false
Debug build with DCHECKs. Runs slowly. Accurate symbols for gdb and
base::debug::StackTrace(). Not a common config, might break.
  4. is_debug=false is_official_build=true
Highly optimized. No DCHECKs. May link slowly. After stripping (eu-strip) the
size matches what we ship.

### Install lacros-chrome on a DUT
deploy_chrome will copy the binary to the device and update /etc/chrome_dev.conf
to include the appropriate flags. 
```
% ./third_party/chromite/bin/deploy_chrome --build-dir=out_device_lacros/Release
--device="${your-device-ip}:${port}" --board=${BOARD} --lacros
```
Specifically, if you connect your DUT via SSH’s reverse proxy, you can specify
--device=localhost:8022 (assuming your forwarding port is 8022).

You can now launch your copy of lacros-chrome by clicking on the yellow Lacros
icon in the app list or shelf. Logs go to /home/chronos/user/lacros/lacros.log.
Use LOG() macros to emit logs, or use stdout/stderr. All of them output to the
file.su.

See Appendix if you need to launch lacros-chrome manually from the DUT command
line.

### Controlling Lacros on the DUT
You can control Lacros’s command line to enable features, turn on logging, and
so on by using the --lacros-chrome-additional-args argument to ash-chrome.
Note that you can only use one --lacros-chrome-additional-args; use #### as a
delimiter between args. For example, to tell Lacros to enable feature WebShare
and also set vmodule dbus to 1 and bluez to 3, add the following line to
/etc/chrome_dev.conf:
```
--lacros-chrome-additional-args=--enable-features=WebShare####--vmodule=*dbus*=1,*bluez*=3
```

### Appendix

#### Run lacros-chrome manually on DUT
Add this to your /etc/chrome_dev.conf:
```
--lacros-mojo-socket-for-testing=/tmp/lacros.sock
```
And then run 'restart ui' to restart ash-chrome.

To copy the necessary files to your dut (after each time you build), you can use
the simplechrome (deploy_chrome) flow above. Or, if you need to copy the files
manually:
```
# Maybe optional! See text above.
% rsync --progress -r out_device_lacros/Release/{chrome,nacl_helper, \
	nacl_irt_x86_64.nexe,locales,*.so,*.pak,icudtl.dat,snapshot_blob.bin, \
	crashpad_handler,swiftshader,WidevineCdm} root@${DUT}:/usr/local/lacros-chrome
```
If you are using a test image, mojo_connection_lacros_launcher.py should exist.
If not, from a Chrome checkout, grab the
build/lacros/mojo_connection_lacros_launcher.py script:
```
# Maybe optional! See text above.
scp ./build/lacros/mojo_connection_lacros_launcher.py ${DUT}:/usr/local/bin/
```

Then launch lacros:
```
% ssh ${DUT}
localhost ~ # su chronos
localhost ~ # cd /usr/local/lacros-chrome
localhost /usr/local/lacros-chrome # mkdir user_data
localhost /usr/local/lacros-chrome # EGL_PLATFORM=surfaceless \
XDG_RUNTIME_DIR=/run/chrome python \
/usr/local/bin/mojo_connection_lacros_launcher.py -s /tmp/lacros.sock ./chrome \
--ozone-platform=wayland --user-data-dir=/usr/local/lacros-chrome/user_data \
--enable-gpu-rasterization --enable-oop-rasterization \
--enable-webgl-image-chromium --lang=en-US \
--breakpad-dump-location=/usr/local/lacros-chrome/
```
You may be interested in setting WAYLAND_DEBUG=1 env var to see all Wayland \
protocol messages. This can also be set via /etc/chrome_dev.conf:
```
--lacros-chrome-additional-env=WAYLAND_DEBUG=1
```

Add the flag --disable-gpu to run using SwiftShader [software rendering] \
rather than hardware acceleration.

To launch lacros with gdb, same as above but with `gdb --args` spliced in:

```
localhost /usr/local/lacros-chrome # EGL_PLATFORM=surfaceless \
XDG_RUNTIME_DIR=/run/chrome python3 \
/usr/local/bin/mojo_connection_lacros_launcher.py -s /tmp/lacros.sock gdb \
--args ./chrome --ozone-platform=wayland \
--user-data-dir=/usr/local/lacros-chrome/user_data --enable-gpu-rasterization \
--enable-oop-rasterization --enable-webgl-image-chromium --lang=en-US \
--breakpad-dump-location=/usr/local/lacros-chrome/
```

#### Running content_shell on device

Two files need to be copied: content_shell and content_shell.pak

One modification: set chromeos/dbus/config/use_real_dbus_clients.gni to false.
