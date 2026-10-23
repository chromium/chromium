---
name: ios/inspect-headers-via-nsurlsession-data-delegate
description: When inspecting HTTP headers or resource metadata with NSURLSession, implement NSURLSessionDataDelegate didReceiveResponse to cancel the task early instead of dataTaskWithRequest completionHandler
rubric: safety
severity: should_fix
---

**Rule.** When inspecting HTTP response headers or metadata (such as `MIMEType`,
`Content-Type`, or `Content-Length`) before or without downloading the response
body, configure an `NSURLSession` with an `NSURLSessionDataDelegate`
implementing `-URLSession:dataTask:didReceiveResponse:completionHandler:` and
invoke `completionHandler(NSURLSessionResponseCancel)` to abort the transfer
early. Do not use `-[NSURLSession dataTaskWithRequest:completionHandler:]`,
which buffers the entire response body in memory before invoking its completion
block.

**Why.** Convenience completion handlers on `NSURLSessionDataTask` download and
buffer the full response body into memory before calling the handler, wasting
network bandwidth and causing memory spikes or out-of-memory terminations in
memory-constrained processes like app extensions. Implementing
`NSURLSessionDataDelegate`'s response callback delivers HTTP headers as soon as
they arrive from the server, allowing the client to inspect the metadata and
immediately terminate the connection before payload data transfer begins.

## Examples

Bad:

```cpp
// BAD: dataTaskWithRequest:completionHandler: downloads the entire response
// body before the completion block is invoked, wasting memory and bandwidth.
NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:URL];
__weak __typeof(self) weakSelf = self;
NSURLSessionDataTask* task = [[NSURLSession sharedSession]
    dataTaskWithRequest:request
      completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
        if ([response.MIMEType hasPrefix:@"image/"]) {
          [weakSelf handleImageAtURL:URL];
        }
      }];
[task resume];
```

Good:

```cpp
// GOOD: NSURLSessionDataDelegate receives headers early and cancels transfer
// via NSURLSessionResponseCancel before downloading the response body.
- (void)URLSession:(NSURLSession*)session
              dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveResponse:(NSURLResponse*)response
     completionHandler:(void (^)(NSURLSessionResponseDisposition))completionHandler {
  NSString* MIMEType = response.MIMEType;
  completionHandler(NSURLSessionResponseCancel);
  if ([MIMEType hasPrefix:@"image/"]) {
    [self handleImageAtURL:dataTask.originalRequest.URL];
  }
}
```
