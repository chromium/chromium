# Updating a Benchmark Owner

## Who is a benchmark owner?
A benchmark owner is the main point of contact for a given benchmark. The owner is responsible for:

- Triaging breakages and ensuring they are fixed
- Following up when CL authors who regressed performance have questions about the benchmark and metric
- Escalating when a regression is WontFix-ed and they disagree with the decision.

There can be multiple owners of a benchmark, for example if there are multiple types of metrics, or if one owner drives metric development and another drives performance improvements in an area.

## How do I update the owner on a benchmark?

### Telemetry Benchmarks
1. Open [`src/tools/perf/benchmarks/benchmark_name.py`](https://cs.chromium.org/chromium/src/tools/perf/benchmarks/), where `benchmark_name` is the part of the benchmark before the “.”, like `smoothness`  in `smoothness.top_25_smooth`.
1. Find the class for the benchmark. It has a `Name` method that should match the full name of the benchmark.
1. Add a `benchmark.Info` decorator above the class.

  Example:

  ```
  @benchmark.Info(
      emails=['owner1@chromium.org', 'owner2@samsung.com'],
      component=’GoatTeleporter>Performance’,
      documentation_url='http://link.to/your_benchmark_documentation')
  ```

  In this example, there are two owners for the benchmark, specified by email; a bug component,
  which will be automatically added to the bug by the perf dashboard; and a link
  to documentation (which will be added to regression bugs in Q3 2018).

1. Run `tools/perf/generate_perf_data` to update `tools/perf/benchmark.csv`.
1. Upload the benchmark python file and `benchmark.csv` to a CL for review. Please add any previous owners to the review.

### C++ Perf Benchmarks
1. Open [`src/tools/perf/core/perf_data_generator.py`](https://cs.chromium.org/chromium/src/tools/perf/core/perf_data_generator.py).
1. Find the BenchmarkMetadata for the benchmark. It will be in a dictionary named `GTEST_BENCHMARKS`.
1. Update the email (first field of `BenchmarkMetadata`).
1. Run `tools/perf/generate_perf_data` to update `tools/perf/benchmark.csv`.
1. Upload `perf_data_generator.py` and `benchmark.csv` to a CL for review. Please add any previous owners to the review.
