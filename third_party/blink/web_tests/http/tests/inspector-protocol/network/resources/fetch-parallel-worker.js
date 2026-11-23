self.onmessage = (event) => {
  Promise
      .all(event.data.map(url => {
        return fetch(url).then((res) => res.text());
      }))
      .then(results => {
        self.postMessage(results);
      });
};
