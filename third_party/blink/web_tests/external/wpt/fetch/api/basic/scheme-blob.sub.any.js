// META: script=../resources/utils.js

function checkFetchResponse(url, data, mime, size, desc) {
  promise_test(function(test) {
    size = size.toString();
    return fetch(url).then(function(resp) {
      assert_equals(resp.status, 200, "HTTP status is 200");
      assert_equals(resp.type, "basic", "response type is basic");
      assert_equals(resp.headers.get("Content-Type"), mime, "Content-Type is " + resp.headers.get("Content-Type"));
      assert_equals(resp.headers.get("Content-Length"), size, "Content-Length is " + resp.headers.get("Content-Length"));
      return resp.text();
    }).then(function(bodyAsText) {
      assert_equals(bodyAsText, data, "Response's body is " + data);
    });
  }, desc);
}

var blob = new Blob(["Blob's data"], { "type" : "text/plain" });
checkFetchResponse(URL.createObjectURL(blob), "Blob's data", "text/plain",  blob.size,
                  "Fetching [GET] URL.createObjectURL(blob) is OK");

function checkKoUrl(url, method, desc) {
  promise_test(function(test) {
    var promise = fetch(url, {"method": method});
    return promise_rejects_js(test, TypeError, promise);
  }, desc);
}

var blob2 = new Blob(["Blob's data"], { "type" : "text/plain" });
checkKoUrl("blob:http://{{domains[www]}}:{{ports[http][0]}}/", "GET",
          "Fetching [GET] blob:http://{{domains[www]}}:{{ports[http][0]}}/ is KO");

var invalidRequestMethods = [
  "POST",
  "OPTIONS",
  "HEAD",
  "PUT",
  "DELETE",
  "INVALID",
];
invalidRequestMethods.forEach(function(method) {
  checkKoUrl(URL.createObjectURL(blob2), method, "Fetching [" + method + "] URL.createObjectURL(blob) is KO");
});

var empty_blob = new Blob([]);
checkFetchResponse(URL.createObjectURL(empty_blob), "", "", 0,
                  "Fetching URL.createObjectURL(empty_blob) is OK");

var empty_type_blob = new Blob([], {type: ""});
checkFetchResponse(URL.createObjectURL(empty_type_blob), "", "", 0,
                  "Fetching URL.createObjectURL(empty_type_blob) is OK");

var empty_data_blob = new Blob([], {type: "text/plain"});
checkFetchResponse(URL.createObjectURL(empty_data_blob), "", "text/plain", 0,
                  "Fetching URL.createObjectURL(empty_data_blob) is OK");

checkKoUrl("blob:not-backed-by-a-blob/", "GET",
           "Fetching [GET] blob:not-backed-by-a-blob/ is KO");

promise_test(function(test) {
  var blob = new Blob(["content type with invalid character"], {"type": "text/plain"});
  let slice = blob.slice(8, 25, "\0");
  return fetch(URL.createObjectURL(slice)).then(function (resp) {
    assert_equals(resp.status, 200, "HTTP status is 200");
    assert_equals(resp.type, "basic", "response type is basic");
    assert_equals(resp.headers.get("Content-Type"), "");
    assert_equals(resp.headers.get("Content-Length"), "17");
    return resp.text();
  }).then(function(bodyAsText) {
    assert_equals(bodyAsText, "type with invalid");
  });
}, "Set content type to the empty string for slice with invalid content type");

promise_test(function(test) {
  var blob = new Blob(["content type that is empty"], {"type": "text/plain"});
  let slice = blob.slice(8, 20);
  return fetch(URL.createObjectURL(slice)).then(function (resp) {
    assert_equals(resp.status, 200, "HTTP status is 200");
    assert_equals(resp.type, "basic", "response type is basic");
    assert_equals(resp.headers.get("Content-Type"), "");
    assert_equals(resp.headers.get("Content-Length"), "12");
    return resp.text();
  }).then(function(bodyAsText) {
    assert_equals(bodyAsText, "type that is");
  });
}, "Set content type to the empty string for slice with no content type ");

done();
