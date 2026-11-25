// META: title=Compression dictionary request headers are forbidden header names
// META: global=window,worker

for (const name of ["Available-Dictionary", "Dictionary-ID"]) {
  test(() => {
    const request = new Request("https://example.com/", { headers: { [name]: ":AAAA:" } });
    assert_false(request.headers.has(name), "Request constructor must drop " + name);
    request.headers.set(name, ":AAAA:");
    request.headers.append(name, ":AAAA:");
    assert_false(request.headers.has(name), "Request headers must reject " + name);
    assert_equals(request.headers.get(name), null);
  }, name + " is a forbidden request header name");

  test(() => {
    const response = new Response("", { headers: { [name]: "x" } });
    assert_true(response.headers.has(name), name + " is not forbidden for responses");
  }, name + " is allowed in Response headers");

  test(() => {
    const headers = new Headers({ [name]: "x" });
    assert_true(headers.has(name), "Guard none must allow " + name);
  }, name + " is allowed in Headers with guard none");
}

done();
