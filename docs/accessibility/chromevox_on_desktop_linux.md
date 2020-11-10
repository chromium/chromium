# ChromeVox on Desktop Linux

## Starting ChromeVox

On Chrome OS, you can enable spoken feedback (ChromeVox) by pressing Ctrl+Alt+Z.

If you have a Chromebook, this gives you speech support built-in. If you're
building Chrome from source and running it on desktop Linux, speech and braille
won't be included by default. Here's how to enable it.

## Compiling the Chrome OS version of Chrome

First follow the public instructions for
[Chrome checkout and build](https://www.chromium.org/developers/how-tos/get-the-code).

Edit `.gclient` (in `chromium/`) and at the bottom add:

```
target_os = ["chromeos"]
```

Run `gclient sync` to update your checkout.

Then create a GN configuration with "chromeos" as the target OS, for example:

```
gn args out/cros
```

...in editor, add this line:

```
target_os = "chromeos"
is_component_build = true
is_debug = false
```

Note: Only ```target_os = "chromeos"``` is required, the others are recommended
for a good experience but you can configure Chrome however you like otherwise.
Note that Native Client is required, so do not put `enable_nacl = false` in
your file anywhere!

Now build Chrome as usual, e.g.:

```
autoninja -C out/cros chrome
```

And run it as usual to see a mostly-complete Chrome OS desktop inside
of a window:

```
out/cros/chrome
```

By default you'll be logged in as the default user. If you want to
simulate the login manager too, run it like this:

```
out/cros/chrome --login-manager
```

You can run any of the above under it’s own X session (avoiding any window
manager key combo conflicts) by doing something like

```
startx out/cros/chrome
```

NOTE: if you decide to run Chrome OS under linux within a window manager, you
are subject to its keybindings which will most certainly conflict with
ChromeVox. The Search key (which gets mapped from LWIN/key code 91), usually
gets assigned to numerous shortcut combinations. You can manually disable all
such combinations, or run under X as described above.

## Speech

If you want speech, you just need to copy the speech synthesis data files to
/usr/share like it would be on a Chrome OS device:

```
gsutil ls gs://chromeos-localmirror/distfiles/espeak*
```

Pick the latest version and

```
gsutil cp gs://chromeos-localmirror/distfiles/espeak-ng-20180801.tar.gz /usr/share/chromeos-assets/speech_synthesis/espeak-ng/
tar xvf /usr/share/chromeos-assets/speech_synthesis/espeak-ng/espeak-ng-20180801.tar.gz
rm /usr/share/chromeos-assets/speech_synthesis/espeak-ng/espeak-ng-20180801.tar.gz
```

**Be sure to check permissions of /usr/share/chromeos-assets, some users report
they need to chmod or chown too, it really depends on your system.**

**Note that the default Google tts engine is now only available on an actual
Chrome OS device. **

After you do that, just run "chrome" as above (e.g. out/cros/chrome) and press
Ctrl+Alt+Z, and you should hear it speak! If not, check the logs.

## Braille

ChromeVox uses extension APIs to deliver braille to Brltty through libbrlapi
and uses Liblouis to perform translation and backtranslation.

Once built, Chrome and ChromeVox will use your machine’s running Brltty
daemon to display braille if ChromeVox is running. Simply ensure you have a
display connected before running Chrome and that Brltty is running.

Note you may need to customize brltty.conf (typically found in /etc).
In particular, the api-parameters Auth param may exclude the current user.
You can turn this off by doing:
api-parameters Auth=none

Testing against the latest releases of Brltty (e.g. 5.4 at time of writing) is
encouraged.

For more general information, see [ChromeVox](chromevox.md)

# Using ChromeVox

ChromeVox keyboard shortcuts use Search. On Linux that's usually your Windows
key. If some shortcuts don't work, you may need to remove Gnome keyboard
shortcut bindings, or use "startx", as suggested above, or remap it.

* Search+Space: Click
* Search+Left/Right: navigate linearly
* Search+Period: Open ChromeVox menus
* Search+H: jump to next heading on page
