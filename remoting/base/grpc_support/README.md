## Using GrpcAsyncExecutor

gRPC++ uses
[completion queue](https://grpc.io/docs/tutorials/async/helloasync-cpp.html)
to handle async operations. It doesn't fit well with Chromium's callback-based
async handling paradigm, and it is technically still a blocking API unless you
create a new thread to run the queue. gRPC is working on adding callback-based
APIs but it won't be ready to use in the near future, so we created a
GrpcAsyncExecutor class to help adapting gRPC's completion queue logic into
Chromium's callback paradigm.

### Basic usage

```cpp
class MyClass {
  public:
  MyClass() = default;
  ~MyClass() = default;

  void SayHello() {
    HelloRequest request;
    // Requests will be silently dropped once the executor is destroyed, so it's
    // safe to bind with raw pointers.
    auto grpc_request = CreateGrpcAsyncUnaryRequest(
        base::BindOnce(&HelloService::Stub::AsyncSayHello,
                       base::Unretained(stub_.get())),
        request,
        base::BindOnce(&MyClass::OnHelloResult,
                       base::Unretained(this)));

    // Optional. The library sets a default deadline for you if it is not set.
    SetDeadline(grpc_request->context(), kSomeDeadline);

    executor_->ExecuteRpc(std::move(grpc_request));
  }

  void StartHelloStream() {
    StreamHelloRequest request;
    auto grpc_Request = CreateGrpcAsyncServerStreamingRequest(
        base::BindOnce(&HelloService::Stub::AsyncStreamHello,
                       base::Unretained(stub_.get())),
        request,
        base::BindRepeating(&MyClass::OnHelloStreamMessage,
                            base::Unretained(this)),
        base::BindOnce(&MyClass::OnHelloStreamClosed,
                       base::Unretained(this)),
        &scoped_hello_stream_);
    executor_->ExecuteRpc(std::move(grpc_request));
  }

  void CloseHelloStream() {
    scoped_hello_stream_.reset();
  }

  private:
  void OnHelloResult(const grpc::Status& status,
                     const HelloResponse& response) {
    if (!status.ok()) {
      // Handle error here.
      return;
    }

    // Response is received. Use the result here.
  }

  void OnHelloStreamMessage(const HelloStreamResponse& response) {
    // This will be called every time the server sends back messages
    // through the stream.
  }

  void OnHelloStreamClosed(const grpc::Status& status) {
    if (!status.ok()) {
      // Handle error here.
      return;
    }

    // Stream is closed by the server.
  }

  std::unique_ptr<HelloService::Stub> stub_;
  GrpcAsyncExecutor executor_;
  std::unique_ptr<ScopedGrpcServerStream> scoped_hello_stream_;
};
```

## Using GrpcAuthenticatedExecutor

GrpcAuthenticatedExecutor allows you to execute RPCs and authenticate them
with `remoting::OAuthTokenGetter`. The usage is basically the same as
`GrpcAsyncExecutor`.
