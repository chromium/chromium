(async() => {
  const response = await fetch("/common/dummy.xml");
  const {readable, writable} = new TransformStream({
    start() {},
    transform() {
      window.busy_wait();
    }
  });
  response.body.pipeTo(writable);
  await readable.getReader().read();
})();
