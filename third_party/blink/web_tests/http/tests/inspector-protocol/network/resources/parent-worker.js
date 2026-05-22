onmessage = (e) => {
  const workerUrl = e.data;
  const worker = new Worker(workerUrl);
  worker.onmessage = () => {
    postMessage('worker loaded successfully');
  };
  worker.onerror = (err) => {
    err.preventDefault();
    postMessage('worker failed to load');
  };
};
