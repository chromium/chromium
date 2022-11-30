# Build linux-Lacros

## System requirements
The same as [linux](../linux/build_instructions.md)

## Setting up output directories to avoid confusion
First follow [linux build instructions](../linux/build_instructions.md) for
depot tools, code checkout, etc.

The following sections assume you can already build Chrome on linux.

Modify your chromium/.gclient to include target_os. It should look like this:
```shell
solutions = [
  {
    "name": "src",
    "url": "https://chromium.googlesource.com/chromium/src.git",
    "managed": False,
    "custom_deps": {},
    "custom_vars": {},
  },
]
target_os=["chromeos"]
```

Then run
```shell
% gclient sync
```

There are several different binaries that can be built from the same repository
using different configurations. To avoid confusion, we recommend using
explicitly named output directories. For example:

out_linux_ash: the directory that holds artifacts for ash-chrome running on linux
```shell
% gn args out_linux_ash/Release

target_os = "chromeos"
use_goma = true          # speeds up compilation
```
out_linux_lacros: the directory that holds artifacts for lacros-chrome running on linux
```shell
% gn args out_linux_lacros/Release

target_os="chromeos"
chromeos_is_browser_only=true
use_goma=true             # speeds up compilation
is_component_build=true   # speeds up links
```

Run lacros-chrome plus ash-chrome on Linux
This is not a configuration that we ship to users, but it's convenient for CrOS
developers to run and test the CrOS UI on Linux.

Make a new, empty XDG runtime dir:
```shell
% mkdir /tmp/ash_chrome_xdg_runtime
```

Build ash-chrome with typical target_os="chromeos" workflow
```shell
% mkdir -p out_linux_ash/Release
% echo '
use_goma=true
target_os="chromeos"' > out_linux_ash/Release/args.gn
% gn gen out_linux_ash/Release
% autoninja -C out_linux_ash/Release chrome
```
See [public doc](../chromeos_build_instructions.md) for more details.

Build lacros-chrome-on-linux:
```shell
% mkdir -p out_linux_lacros/Release
% echo '
chromeos_is_browser_only=true
use_goma=true
target_os="chromeos"
is_component_build=true' > out_linux_lacros/Release/args.gn
% gn gen out_linux_lacros/Release
% autoninja -C out_linux_lacros/Release chrome
```

Clear the old user-data-dir as that can cause problems if it has old data.
You can skip this step to retain data between runs, but if there are any
launch issues be sure to run this step.
```shell
% rm -rf /tmp/ash-chrome
```

Run ash-chrome-on-linux with support for lacros-chrome:
```shell
% XDG_RUNTIME_DIR=/tmp/ash_chrome_xdg_runtime ./out_linux_ash/Release/chrome --user-data-dir=/tmp/ash-chrome --enable-wayland-server --no-startup-window --login-manager --login-profile=user --enable-features=LacrosSupport,LacrosPrimary,LacrosOnly --lacros-chrome-path=${PWD}/out_linux_lacros/Release/
```

You will be prompted to log in. Once you log in, Lacros will be the primary
browser. You can verify this by opening the browser and navigation to
chrome://version. It should report OS:Linux.

The log file for the lacros-chrome-on-linux instance can be found at
${user_data_dir}/lacros/lacros.log, where ${user_data_dir} is set via
--user-data-dir=/tmp/ash-chrome in the command line.

More configuration options
To pass command line flags to the lacros browser use
--lacros-chrome-additional-args
e.g. --lacros-chrome-additional-args=--enable-features=Foo,Bar.
For multiple flags, separate them with ####,
e.g. --lacros-chrome-additional-args=--enable-features=Foo####--switch-bar.

Alternatively, you can launch lacros-chrome-on-linux directly with the help
of the mojo_connection_lacros_launcher.py script (without the script, mojo
connection won’t be hooked up correctly). Firstly, launch ash-chrome-on-linux
with the --lacros-mojo-socket-for-testing cmd line argument.
```shell
% XDG_RUNTIME_DIR=/tmp/ash_chrome_xdg_runtime ./out_linux_ash/Release/chrome --user-data-dir=/tmp/ash-chrome --enable-wayland-server --no-startup-window --login-manager --enable-features=LacrosSupport --lacros-mojo-socket-for-testing=/tmp/lacros.sock
```

Then, launch lacros-chrome-on-linux with the launcher script with
-s (--socket-path) pointing to the same socket path used to launch ash-chrome,
and it’s especially useful when you want to launch lacros-chrome-on-linux
inside a debugger.
```shell
% EGL_PLATFORM=surfaceless XDG_RUNTIME_DIR=/tmp/ash_chrome_xdg_runtime ./build/lacros/mojo_connection_lacros_launcher.py -s /tmp/lacros.sock ./out_linux_lacros/Release/chrome --user-data-dir=/tmp/lacros-chrome
```
