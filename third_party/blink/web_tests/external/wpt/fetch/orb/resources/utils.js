function header(name, value) {
  return `header(${name},${value})`;
}

function contentType(type) {
  return header("Content-Type", type);
}

function fetchORB(file, options, ...pipe) {
  return fetch(`${file}${pipe.length ? `?pipe=${pipe.join("|")}` : ""}`, {
    ...(options || {}),
    mode: "no-cors",
  });
}
