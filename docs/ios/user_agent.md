# User Agent in Chrome for iOS

The User Agent (UA) in Chrome for iOS is the same as the Mobile Safari
user agent, with `CriOS/<ChromeRevision>` instead of
`Version/<VersionNum>`.

Here’s an example of the **Chrome** UA on iPhone:

```
Mozilla/5.0 (iPhone; CPU iPhone OS 10_3 like Mac OS X)
AppleWebKit/602.1.50 (KHTML, like Gecko) CriOS/56.0.2924.75
Mobile/14E5239e Safari/602.1
```

For comparison, the **Mobile Safari** UA:

```
Mozilla/5.0 (iPhone; CPU iPhone OS 10_3 like Mac OS X)
AppleWebKit/603.1.23 (KHTML, like Gecko) Version/10.0
Mobile/14E5239e Safari/602.1
```

When the Request Desktop Site feature is enabled, the UA is the same as
**Desktop Safari** with `CriOS/<ChromeMajorRevision>` being added:

```
Mozilla/5.0 (Macintosh; Intel Mac OS X 10_13_5)
AppleWebKit/605.1.15 (KHTML, like Gecko) CriOS/85
Version/11.1.1 Safari/605.1.15
```
