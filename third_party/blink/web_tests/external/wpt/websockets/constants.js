//This file requires server-side substitutions and must be included as constants.js?pipe=sub

var PORT = "{{ports[ws][0]}}";
var PORT_SSL = "{{ports[wss][0]}}";
var PORT_H2 = "{{ports[h2][0]}}";

function url_has_variant(variant) {
  let params = new URLSearchParams(location.search);
  return params.get(variant) === "";
}

function url_has_flag(flag) {
  let params = new URLSearchParams(location.search);
  return params.getAll("wpt_flags").indexOf(flag) !== -1;
}



var SCHEME_DOMAIN_PORT;
if (url_has_variant('wss')) {
  SCHEME_DOMAIN_PORT = 'wss://{{host}}:' + PORT_SSL;
} else if (url_has_flag('h2')) {
  SCHEME_DOMAIN_PORT = 'wss://{{host}}:' + PORT_H2;
} else {
  SCHEME_DOMAIN_PORT = 'ws://{{host}}:' + PORT;
}
