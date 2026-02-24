function callGeo() {
  navigator.geolocation.getCurrentPosition(() => {}, () => {});
}

callGeo();
