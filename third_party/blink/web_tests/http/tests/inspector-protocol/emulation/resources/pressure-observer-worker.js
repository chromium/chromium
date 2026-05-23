onmessage = async function(e) {
  const observer = new PressureObserver((records) => {
    const flattenedRecords = [];
    records.forEach((record) => {
      flattenedRecords.push([record.source, record.state]);
    });

    postMessage({records: flattenedRecords});

    observer.disconnect();
    close();
  });
  await observer.observe('cpu');
};
