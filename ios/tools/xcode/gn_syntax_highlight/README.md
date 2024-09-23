# GN syntax highlight support for Xcode

This project aims to bring syntax highlight support for GN (`BUILD.gn`) in
Xcode.

> Warning: This package works with Xcode 15.4 (at the moment of writing).
This is not guaranteed to stay compatible with future versions of Xcode.


### Installation:

Close Xcode (_secondary-click + quit_). Run `./install.sh` in the terminal.
Reopen Xcode - if asked by a system alert, accept to load the code bundle.

### To Uninstall:
Delete the previously created files at `~/Library/Developer/Xcode/Plug-ins` and
`~/Library/Developer/Xcode/Specifications`