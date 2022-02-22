// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js

// https://www.w3.org/TR/geolocation-API/

idl_test(
  ['geolocation'],
  ['hr-time', 'html'],
  idl_array => {
    idl_array.add_objects({
      Navigator: ['navigator'],
      Geolocation: ['navigator.geolocation'],
    });
  }
);
