# Debugging UI

The login and lock screen can run with a developer overlay that make common
operations such as setting up a device with 10+ users trivial. Pass the flag
`--show-login-dev-overlay` when running chrome and the UI will automatically
appear.

```sh
./out/Release/chrome --show-login-dev-overlay
```

The overlay will use fake data where necessary to show the relevant UI; buttons
are mostly functional but may break since fake data may be sent to chrome.