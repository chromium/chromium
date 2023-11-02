# Chrome Logging on Chrome OS

## Locations

Messages written via the logging macros in [base/logging.h] end up in different
locations depending on Chrome's state:

`/var/log/ui/ui.LATEST`
:   contains data written to stdout and stderr by Chrome (and technically also
    [session_manager]). This generally comprises messages that are written very
    early in Chrome's startup process, before logging has been initialized.

`/var/log/chrome/chrome`
:   contains messages that are written before a user has logged in. It also
    contains messages written after login on test images, where Chrome runs with
    `--disable-logging-redirect`.

`/home/chronos/user/log/chrome`
:   contains messages that are written while a user is logged in on non-test
    images. Note that this path is within the user's encrypted home directory
    and is only accessible while the user is logged in.

`/var/log/audit/audit.log`:
:   contains SECCOMP violation messages.

`/var/log/messages`
:   contains messages written by services such as `session_manager`,
    `cryptohomed` and `cros-disks` that may be useful in determining when or why
    Chrome started or stopped.

Some of the above files are actually symlinks. Older log files can be found
alongside them in the same directories.

## How to increase Chrome's log level to `INFO` on a test device

By default, only `WARNING`, `ERROR` and `FATAL` messages are written to disk,
whereas `INFO` and `VERBOSE` messages are discarded.

To log `INFO` messages, pass `--log-level=0` to Chrome. See the
[Passing Chrome flags from session_manager] document for more details, and
specifically the `/etc/chrome_dev.conf` configuration file that can be used to
change flags on test devices.

Remount the root filesystem in read-write mode (to be able to modify
`chrome_dev.conf`):

```sh
(dut)$ sudo mount -o remount,rw /
```

Add `--log-level=0` to `chrome_dev.conf`:

```sh
(dut)$ echo "--log-level=0" | sudo tee -a /etc/chrome_dev.conf > /dev/null
```

Restart Chrome:

```sh
(dut)$ sudo restart ui
```

Follow Chrome's logs:

```sh
(dut)$ tail -F /var/log/chrome/chrome
```

## Verbose Logging

When actively debugging issues, Chrome's `--vmodule` flag can be used to log
verbose messages for particular modules.

For example, to log `VERBOSE1` messages produced by `VLOG(1)` in
`volume_manager.cc` or `volume_manager.h`:

```sh
(dut)$ echo "--vmodule=volume_manager=1" | sudo tee -a /etc/chrome_dev.conf > /dev/null
```

Restart Chrome:

```sh
(dut)$ sudo restart ui
```

Follow `volume_manager`'s logs:

```sh
(dut)$ tail -F /var/log/chrome/chrome | grep volume_manager
```

[base/logging.h]: ../base/logging.h
[session_manager]: https://chromium.googlesource.com/chromiumos/platform2/+/main/login_manager/
[Passing Chrome flags from session_manager]: https://chromium.googlesource.com/chromiumos/platform2/+/main/login_manager/docs/flags.md
