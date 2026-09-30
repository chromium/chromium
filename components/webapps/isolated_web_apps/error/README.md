# IWA Error Types & UMA Logging (`components/webapps/isolated_web_apps/error`)

This directory defines structured error types for Signed Web Bundle reading and
UMA metric helpers for Isolated Web Apps.

## Key Components

- `UnusableSwbnFileError` (`unusable_swbn_file_error.{h,cc}`): Structured error
  type categorizing integrity block, signature verification, and metadata
  parsing failures when opening a `.swbn` file.
- `uma_logging.{h,cc}`: Defines `UmaLogExpectedStatus()` and `ToErrorEnum()`
  helpers for recording `base::expected<T, E>` success rates and error enums
  (including `UnusableSwbnFileError`) to UMA histograms.
