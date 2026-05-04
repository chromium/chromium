# Chrome for Testing Configuration

Chrome for Testing (CfT) introduces a configuration file that allows users to
customize certain aspects of its operation. This is particularly useful for
automated testing environments where specific setups are required before the
browser starts.

## Command-Line Switch

To use a configuration file, start Chrome for Testing with the
`--chrome-for-testing-config` switch, passing the path to your JSON
configuration file:

```bash
chrome --chrome-for-testing-config=/path/to/config.json
```

**Switch Usage Notes:**
- **Switch omitted**: If the `--chrome-for-testing-config` switch is not
  specified, Chrome for Testing runs with the default configuration.
- **Path omitted**: If the switch is specified but no file path is provided
  (i.e., just `--chrome-for-testing-config`), Chrome for Testing will reuse the
  configuration that was stored in the user data directory during a previous
  run.

## JSON Configuration Schema

The configuration file is a JSON object. The parser supports standard C-style
comments (`//` and `/* ... */`) and trailing commas, making it easier to
document and maintain the file.

The following keys are supported:

| Key                               | Type             | Default | Description                                                                                                                                                |
| :-------------------------------- | :--------------- | :------ | :--------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `enableUserEducationUI`           | boolean          | `false` | Enables or disables the User Education UI (tutorials, help bubbles).                                                                                       |
| `enableSearchEngineChoiceDialog`  | boolean          | `false` | Enables or disables the search engine choice dialog on startup.                                                                                            |
| `enableVirtualClipboard`          | boolean          | `false` | Enables or disables the virtual clipboard (a process-specific clipboard that is independent of the platform clipboard).                                    |
| `requiredComponents`              | array of strings | None    | A list of component names or patterns (supporting `?` and `*` wildcards) that must be installed and ready before the browser finishes starting.            |
| `requiredComponentsDir`           | string           | None    | Path to override the directory where components are installed. Can be absolute or relative (resolved relative to the browser's current working directory). |
| `requiredComponentsUpdateTimeout` | string           | `"15s"` | Timeout duration for waiting for required components to be ready (e.g., "30s", "1m"). Min: 1s, Max: 5m.                                                    |

### Required Components

The `requiredComponents` feature ensures that specific Chrome components are
downloaded and ready for use before the browser becomes interactive. This
prevents tests from failing or behaving flakily due to missing components that
are usually downloaded asynchronously.

By default, normal background component updates are disabled in Chrome for
Testing. Unless you explicitly specify components in the `requiredComponents`
list, no components will be updated automatically. In this case, the only
components available will be those that are bundled directly with Chrome for
Testing (such as `Hyphenation` and `WidevineCdm`).

#### Component Names and Wildcards

You can specify the name of the component or a pattern. The following standard
wildcards are supported:
- `*` matches zero or more characters.
- `?` matches any single character.

For example:
- To require all components: `["*"]`
- To require a specific component: `["Hyphenation"]`
- To require components starting with "Widevine": `["Widevine*"]`

#### Component Installation Directory

By default, components are installed within the browser's user data directory.
In most automated testing scenarios, a new, temporary user data directory is
created for each test run. This means that any required components would be
downloaded every time the tests run, increasing execution time and network
usage.

To avoid this, it is highly recommended to use the `requiredComponentsDir`
setting to specify a persistent directory outside the user data directory.
This allows components to be shared across test runs and prevents redundant
downloads.

#### Discovering Component Names

To discover the names of the available components, you can run Chrome with the
`--log-level=1 --enable-logging=stderr` command line switches while requesting
all components via the `"requiredComponents": ["*"]` wildcard in your
configuration. The updated component names will be printed directly to the
console log.

#### Component Version Availability

It is not guaranteed that all historical versions of a component will remain
available for download from the production component update servers. If your
tests rely on a specific, older version of a component, the component updater
might not be able to fetch it.

**Recommendation:** To work around this, it is recommended to save the
required version of the component along with your persistent test data. You
can use the `requiredComponentsDir` setting to specify the location where the
browser should discover and load these pre-saved components.

#### Behavior

- **Startup Block**: Chrome will block startup until all required components
  are successfully updated/installed or the timeout is reached.
- **Fail Fast**: If a component fails to update or the timeout expires, Chrome
  will terminate with a fatal error. This ensures that tests do not run in an
  incomplete state.

### Example Configuration

```json
{
  "enableUserEducationUI": false,
  "enableSearchEngineChoiceDialog": false,
  "enableVirtualClipboard": true,
  "requiredComponentsDir": "/tmp/chrome-components",
  "requiredComponentsUpdateTimeout": "60s",
  "requiredComponents": [
    "Safety Tips",
    "Captcha Providers"
  ]
}
```

## Strict Validation

The configuration parser performs strict validation. If the JSON file contains
any keys not listed above, or if the values are of the wrong type, Chrome will
log an error and fail to start. This helps catch typos in your configuration
file early.
