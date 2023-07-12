# Linux Password Storage

On Linux, Chromium can store passwords in five ways:

*   GNOME Libsecret
*   KWallet 4
*   KWallet 5
*   KWallet 6
*   plain text

Chromium chooses which store to use automatically, based on your desktop
environment.

Passwords stored in KWallet are encrypted on disk, and access
to them is controlled by dedicated daemon software. Passwords stored in plain
text are not encrypted. Because of this, when KWallet is
in use, any unencrypted passwords that have been stored previously are
automatically moved into the encrypted store.

Support for using KWallet was added in version 6, but using
these (when available) was not made the default mode until version 12.

## Details

Although Chromium chooses which store to use automatically, the store to use can
also be specified with a command line argument:

*   `--password-store=gnome-libsecret` (to use GNOME Libsecret)
*   `--password-store=kwallet` (to use KWallet 4)
*   `--password-store=kwallet5` (to use KWallet 5)
*   `--password-store=kwallet6` (to use KWallet 6)
*   `--password-store=basic` (to use the plain text store)

Note that Chromium will fall back to `basic` if a requested or autodetected
store is not available.

In versions 6-11, the store to use was not detected automatically, but detection
could be requested with an additional argument:

*   `--password-store=detect`
