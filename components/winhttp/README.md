//components/winhttp

This component is a wrapper around the Microsoft Windows WinHTTP API.  This
component is meant to be used in contexts where Chrome's regular network stack
is not available, like in the Chrome updater and installer.

This component is designed to have minimal dependencies.  For the moment, it
depends only on //base and //url.  It's not anticipated that more dependencies
will be required.  To keep things simple, new dependencies are not desired.
