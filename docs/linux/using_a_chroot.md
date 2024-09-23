# Using a chroot

If you want to run web tests and you're not running Lucid, you'll get errors
due to version differences in libfreetype. To work around this, you can use a
chroot.

## Basic Instructions

*   Run `build/install-chroot.sh`. On the prompts, choose to install a 64-bit
    Lucid chroot and activate all your secondary mount points.
*   sudo edit `/etc/schroot/mount-lucid64bit` and uncomment `/run` and
    `/run/shm`.  Verify that your mount points are correct and uncommented: for
    example, if you have a second hard drive at `/src`, you should have an entry
    like `/src /src none rw,bind 0 0`.
*   Enter your chroot as root with `sudo schroot -c lucid64`.
    Run `build /install-build-deps.sh`, then exit the rooted chroot.
*   Delete your out/ directory if you had a previous non-chrooted build.
*   To enter your chroot as normal user, run `schroot -c lucid64`.
*   Now compile and run DumpRenderTree within chroot.

## Tips and Tricks

### NFS home directories

The chroot install will be installed by default in /home/$USER/chroot. If your
home directory is inaccessible by root (typically because it is mounted on NFS),
then move this directory onto your local disk and change the corresponding entry
in `/etc/schroot/mount-lucid64bit`.

### Reclient builds

If you get mysterious compile errors (glibconfig.h or dbus header error), don't
use reclient for builds inside the chroot.

### Different color prompt

I use the following code in my .zshrc file to change the color of my prompt in
the chroot.

```shell
# load colors
autoload colors zsh/terminfo
if [[ "$terminfo[colors]" -ge 8 ]]; then
  colors
fi
for color in RED GREEN YELLOW BLUE MAGENTA CYAN WHITE; do
  eval PR_$color='%{$terminfo[bold]$fg[${(L)color}]%}'
  eval PR_LIGHT_$color='%{$fg[${(L)color}]%}'
done
PR_NO_COLOR="%{$terminfo[sgr0]%}"

# set variable identifying the chroot you work in (used in the prompt below)
if [ -z "$debian_chroot" ] && [ -r /etc/debian_chroot ]; then
  debian_chroot=$(cat /etc/debian_chroot)
fi

if [ "xlucid64" = "x$debian_chroot" ]; then
    PS1="%n@$PR_GREEN% lucid64$PR_NO_COLOR %~ %#"
else
    PS1="%n@$PR_RED%m$PR_NO_COLOR %~ %#"
fi
```

### Running X apps

I also have `DISPLAY=:0` in my `$debian_chroot` section so I can run test_shell
or web tests without manually setting my display every time.  Your display
number may vary (`echo $DISPLAY` outside the chroot to see what your display
number is).

You can also use `Xvfb` if you only want to
[run tests headless](web_tests_linux.md#Using-an-embedded-X-server).

### Having web test results open in a browser

After running web tests, you should get a new browser tab or window that
opens results.html.  If you get an error "Failed to open
file:///path/to/results.html, check the
following conditions.

1.  Make sure `DISPLAY` is set. See the
    [Running X apps](#Running-X-apps) section above.
1.  Install `xdg-utils`, which includes `xdg-open`, a utility for finding the
    right application to open a file or URL with.
1.  Install [Chrome](https://www.google.com/intl/en/chrome/browser/).
