# Linux SUID Sandbox Development

*IMPORTANT NOTE: The Linux SUID sandbox is almost but not completely removed.
See https://bugs.chromium.org/p/chromium/issues/detail?id=598454
This page is mostly out-of-date.*

For context see [LinuxSUIDSandbox](linux_suid_sandbox.md)

We need a SUID helper binary to turn on the sandbox on Linux.

In most cases, you can run `build/update-linux-sandbox.sh` and it'll install
the proper sandbox for you in `/usr/local/sbin` and tell you to update your
`.bashrc` if needed.

## Installation instructions for developers

*   If you have no setuid sandbox at all, you will see a message such as:

    ```
    Running without the SUID sandbox!
    ```

*   If your setuid binary is out of date, you will get messages such as:

    ```
    The setuid sandbox provides API version X, but you need Y
    You are using a wrong version of the setuid binary!
    ```

Run the script mentioned above, or do something such as:

*   Build `chrome_sandbox` whenever you build chrome
    (`ninja -C xxx chrome chrome_sandbox` instead of `ninja -C xxx chrome`)
*   After building, run something similar to (or use the provided
    `update-linux-sandbox.sh`):

    ```shell
    # needed if you build on NFS!
    sudo cp out/Debug/chrome_sandbox /usr/local/sbin/chrome-devel-sandbox
    sudo chown root:root /usr/local/sbin/chrome-devel-sandbox
    sudo chmod 4755 /usr/local/sbin/chrome-devel-sandbox
    ```

*   Put this line in your `~/.bashrc` (or `.zshenv` etc):

    ```
    export CHROME_DEVEL_SANDBOX=/usr/local/sbin/chrome-devel-sandbox
    ```

## Try bots and waterfall

If you're installing a new bot, always install the setuid sandbox (the
instructions are different than for developers, contact the Chrome troopers). If
something does need to run without the setuid sandbox, use the
`--disable-setuid-sandbox` command line flag.

The `SUID` sandbox must be enabled on the try bots and the waterfall. If you
don't use it locally, things might appear to work for you, but break on the
bots.

(Note: as a temporary, stop gap measure, setting `CHROME_DEVEL_SANDBOX` to an
empty string is equivalent to `--disable-setuid-sandbox`)

## Disabling the sandbox

If you are certain that you don't want the setuid sandbox, use
`--disable-setuid-sandbox`. There should be very few cases like this. So if
you're not absolutely sure, run with the setuid sandbox.

## Installation instructions for "[Raw builds of Chromium](https://commondatastorage.googleapis.com/chromium-browser-continuous/index.html)"

If you're using a "raw" build of Chromium, do the following:

    sudo chown root:root chrome_sandbox && sudo chmod 4755 chrome_sandbox && \
        export CHROME_DEVEL_SANDBOX="$PWD/chrome_sandbox"
    ./chrome

You can also make such an installation more permanent by following the
[steps above](#Installation-Instructions-for-developers) and installing
`chrome_sandbox` to a more permanent location.

## System-wide installations of Chromium

The `CHROME_DEVEL_SANDBOX` variable is intended for developers and won't work
for a system-wide installation of Chromium. Package maintainers should make sure
the `setuid` binary is installed.
