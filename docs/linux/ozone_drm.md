# Running ChromeOS UI on Linux
Note that this instructions may not work for you. They have been
verified to work as of 2018/06/06 on standard Google engineering
workstations as issued to engineerings on the Chrome team. Please
submit patches describing the steps needed for other machines or distributions.

## Nouveau
If you have an NVidia card, you probably have the binary drivers installed. These install a blacklist for the nouveau kernel modules. Best is to remove the nvidia driver and switch to nouveau completely:

```
$ sudo apt-get remove --purge "nvidia*"
$ sudo apt-get install xserver-xorg-input-evdev xserver-xorg-input-mouse xserver-xorg-input-kbd xserver-xorg-input-libinput xserver-xorg-video-nouveau
$ sudo dpkg-reconfigure xserver-xorg
$ # If you are using a Google development machine:
$ sudo goobuntu-config set custom_video_driver custom
```

Default version of nouveau xorg driver is too old for the NV117 chipset in Z840 machines. Install a newer version:

```
$ cd /tmp
$ wget http://http.us.debian.org/debian/pool/main/x/xserver-xorg-video-nouveau/xserver-xorg-video-nouveau_1.0.15-2_amd64.deb
$ sudo apt-get install ./xserver-xorg-video-nouveau_1.0.15-2_amd64.deb
```

At this point you *must  reboot.* If you run into issues to load video at boot then disable `load_video` and `gfx_mode` in `/boot/grub/grub.cfg`.

## Building Chrome
Checkout chromium as per your usual workflow. See [Get the Code:
Checkout, Build, & Run
Chromium](https://www.chromium.org/developers/how-tos/get-the-code).
Googlers should checkout chromium source code as described here:
[Building Chromium on a corporate Linux
workstation](https://companydoc.corp.google.com/company/teams/chrome/build_instructions.md?cl=head)

We want to build on linux on top of Ozone with the DRM 
(Direct Render Manager) platform which is backed by GBM 
(Generic Buffer Management). The following instructions builds 
chromium targets along with minigbm  that lives in the chromium 
tree `src/third_party/minigbm`. Currently, there is no builder for
this configuration so while this worked (mostly) when this document 
was written, some experimentation may be necessary.

Set the gn args for your output dir target `out/Nouveau` with:

```
$ gn args out/Nouveau
Add the following arguments:
dcheck_always_on = true
use_ozone = true
target_os = "chromeos"
ozone_platform_drm = true
ozone_platform = "drm"
use_system_minigbm = false
target_sysroot = "//build/linux/debian_jessie_amd64-sysroot"
is_debug = false
use_remoteexec = true
use_xkbcommon = true
#use_evdev_gestures = true
#use_system_libevdev = false
#use_system_gestures = false

# Non-Googlers should set the next two flags to false
is_chrome_branded = true
is_official_build = true
use_pulseaudio = false
```

Build official release build of chrome:

```
$ ninja -j768 -l24 -C out/Nouveau chrome chrome_sandbox nacl_helper
$ # Give user access to dri, input and audio device nodes:
$ sudo sh -c "echo 'KERNEL==\"event*\", NAME=\"input/%k\", MODE=\"660\", GROUP=\"plugdev\"' > /etc/udev/rules.d/90-input.rules"
$ sudo sh -c "echo 'KERNEL==\"card[0-9]*\", NAME=\"dri/%k\", GROUP=\"video\"' > /etc/udev/rules.d/90-dri.rules"
$ sudo udevadm control --reload
$ sudo udevadm trigger --action=add
$ sudo usermod -a -G plugdev $USER
$ sudo usermod -a -G video $USER
$ sudo usermod -a -G audio $USER
$ newgrp video
$ newgrp plugdev
$ newgrp audio
$ # Stop pulseaudio if running:
$ pactl exit
```

Run chrome: (Set `CHROMIUM_SRC` to the directory containing your Chrome checkout.)

```
$ sudo chvt 8; EGL_PLATFORM=surfaceless $CHROMIUM_SRC/out/Nouveau/chrome --ozone-platform=drm --enable-running-as-system-compositor --login-profile=user --user-data-dir=$HOME/.config/google-chrome-gbm --use-gl=egl --enable-wayland-server --login-manager --ash-constrain-pointer-to-root --default-tile-width=512 --default-tile-height=512 --system-developer-mode --crosh-command=/bin/bash
```

Login to Chrome settings should synchronize.

Install Secure Shell if not already installed from  [the web store](https://chrome.google.com/webstore/detail/secure-shell/pnhechapfaindjhompbnflcldabbghjo?hl=en)

