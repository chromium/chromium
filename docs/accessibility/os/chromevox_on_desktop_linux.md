# ChromeVox on Desktop Linux

## Starting ChromeVox

On ChromeOS, you can enable spoken feedback (ChromeVox) by pressing Ctrl+Alt+Z.

If you have a Chromebook, this gives you speech support built-in. If you're
building Chrome from source and running it on desktop Linux, speech and braille
won't be included by default. Here's how to enable it.

## Compiling the ChromeOS version of Chrome

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

And run it as usual to see a mostly-complete ChromeOS desktop inside
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

### Remapping keys so ChromeVox recognizes a Search key
ChromeVox expects that the Search key is mapped from your
left Windows key/LWIN/key code 91; however, your window manager/desktop
environment (Linux) treats this as a Super or Meta which usually gets assigned
to numerous shortcut combinations.

#### Option 1: running under a new X session
To avoid these conflicts, run using startx as described above.

#### Option 2: remapping keys in your window manager
If you decide not to run under X or wish to run Linux within a window manager
such as through Chrome Remote Desktop or a virtual machine, you need to remap
keys either in Linux or inside ChromeOS.

To manually disable all conflicting key combinations in Linux, remove all
keyboard bindings that reference "Super" or "Meta" in
System Settings > Keyboard > Shortcuts.

#### Option #3: remapping the Search key inside ChromeOS
To remap the Search key inside ChromeOS, go to Settings > Device > Keyboard.
The control key is a good choice for setting as Search as there should be no
conflicts with Linux on its own. Caps Lock is not recommended to change as
ChromeVox may handle it as a special case.

## Speech

If you want speech, you just need to copy the speech synthesis data files to
/usr/share like it would be on a ChromeOS device:

```
gsutil ls gs://chromeos-localmirror/distfiles/espeak\*
```

Pick the latest version and

```
VERSION=1.51
TMPDIR=$(mktemp -d)
gsutil cp gs://chromeos-localmirror/distfiles/espeak-ng-$VERSION.tar.xz $TMPDIR
mkdir $TMPDIR/extract
tar -C $TMPDIR/extract -xvf $TMPDIR/espeak-ng-$VERSION.tar.xz
sudo mkdir -p /usr/share/chromeos-assets/speech_synthesis/espeak-ng/
sudo chown -R $(whoami) /usr/share/chromeos-assets/
cp -r $TMPDIR/extract/* /usr/share/chromeos-assets/speech_synthesis/espeak-ng
rm -rf $TMPDIR
```

**Be sure to check permissions of /usr/share/chromeos-assets, some users report
they need to chmod or chown too, it really depends on your system.**

**Note that the default Google tts engine is now only available on an actual
ChromeOS device.**

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

Testing against the latest releases of Brltty (e.g. 6.3 at time of writing) is
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
