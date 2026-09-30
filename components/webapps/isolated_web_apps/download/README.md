# IWA Bundle Downloader (`components/webapps/isolated_web_apps/download`)

This directory implements network downloading of Signed Web Bundles (`.swbn`) to
scoped temporary files on disk.

## Key Components (`bundle_downloader.{h,cc}`)

- `ScopedTempWebBundleFile`: Manages the creation and deletion of a temporary
  `.swbn` file on a background `base::ThreadPool` worker thread to avoid
  blocking the UI thread.
- `IsolatedWebAppDownloader`: Resolves the destination host's IP address space
  via `network::SimpleHostResolver` and downloads either the full Signed Web
  Bundle to disk (`DownloadSignedWebBundle()`) or the leading 8 KiB into memory
  (`DownloadInitialBytes()`) using `network::SimpleURLLoader`.
