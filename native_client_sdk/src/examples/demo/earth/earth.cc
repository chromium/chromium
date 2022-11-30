// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <math.h>
#include <ppapi/c/ppb_input_event.h>
#include <ppapi/cpp/input_event.h>
#include <ppapi/cpp/var.h>
#include <ppapi/cpp/var_array.h>
#include <ppapi/cpp/var_array_buffer.h>
#include <ppapi/cpp/var_dictionary.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <string>

#include "ppapi_simple/ps.h"
#include "ppapi_simple/ps_context_2d.h"
#include "ppapi_simple/ps_event.h"
#include "ppapi_simple/ps_interface.h"
#include "sdk_util/macros.h"
#include "sdk_util/thread_pool.h"

using namespace sdk_util;  // For sdk_util::ThreadPool

// Global properties used to setup Earth demo.
namespace {
const float kPI = M_PI;
const float kTwoPI = kPI * 2.0f;
const float kOneOverPI = 1.0f / kPI;
const float kOneOver2PI = 1.0f / kTwoPI;
const float kOneOver255 = 1.0f / 255.0f;
const int kArcCosineTableSize = 4096;
const int kFramesToBenchmark = 100;
const float kZoomMin = 1.0f;
const float kZoomMax = 50.0f;
const float kWheelSpeed = 2.0f;
const float kLightMin = 0.0f;
const float kLightMax = 2.0f;

// Timer helper for benchmarking.  Returns seconds elapsed since program start,
// as a double.
timeval start_tv;
int start_tv_retv = gettimeofday(&start_tv, NULL);

inline double getseconds() {
  const double usec_to_sec = 0.000001;
  timeval tv;
  if ((0 == start_tv_retv) && (0 == gettimeofday(&tv, NULL)))
    return (tv.tv_sec - start_tv.tv_sec) + tv.tv_usec * usec_to_sec;
  return 0.0;
}

// RGBA helper functions, used for extracting color from RGBA source image.
inline float ExtractR(uint32_t c) {
  return static_cast<float>(c & 0xFF) * kOneOver255;
}

inline float ExtractG(uint32_t c) {
  return static_cast<float>((c & 0xFF00) >> 8) * kOneOver255;
}

inline float ExtractB(uint32_t c) {
  return static_cast<float>((c & 0xFF0000) >> 16) * kOneOver255;
}

// BGRA helper function, for constructing a pixel for a BGRA buffer.
inline uint32_t MakeBGRA(uint32_t b, uint32_t g, uint32_t r, uint32_t a) {
  return (((a) << 24) | ((r) << 16) | ((g) << 8) | (b));
}

// simple container for earth texture
struct Texture {
  int width, height;
  uint32_t* pixels;
  Texture(int w, int h) : width(w), height(h) {
    pixels = new uint32_t[w * h];
    memset(pixels, 0, sizeof(uint32_t) * w * h);
  }
  explicit Texture(int w, int h, uint32_t* p) : width(w), height(h) {
    pixels = new uint32_t[w * h];
    memcpy(pixels, p, sizeof(uint32_t) * w * h);
  }

  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;

  ~Texture() { delete[] pixels; }
};



struct ArcCosine {
  // slightly larger table so we can interpolate beyond table size
  float table[kArcCosineTableSize + 2];
  float TableLerp(float x);
  ArcCosine();
};

ArcCosine::ArcCosine() {
  // build a slightly larger table to allow for numeric imprecision
  for (int i = 0; i < (kArcCosineTableSize + 2); ++i) {
    float f = static_cast<float>(i) / kArcCosineTableSize;
    f = f * 2.0f - 1.0f;
    table[i] = acos(f);
  }
}

// looks up acos(f) using a table and lerping between entries
// (it is expected that input f is between -1 and 1)
float ArcCosine::TableLerp(float f) {
  float x = (f + 1.0f) * 0.5f;
  x = x * kArcCosineTableSize;
  int ix = static_cast<int>(x);
  float fx = static_cast<float>(ix);
  float dx = x - fx;
  float af = table[ix];
  float af2 = table[ix + 1];
  return af + (af2 - af) * dx;
}

// Helper functions for quick but approximate sqrt.
union Convert {
  float f;
  int i;
  Convert(int x) { i = x; }
  Convert(float x) { f = x; }
  int AsInt() { return i; }
  float AsFloat() { return f; }
};

inline const int AsInteger(const float f) {
  Convert u(f);
  return u.AsInt();
}

inline const float AsFloat(const int i) {
  Convert u(i);
  return u.AsFloat();
}

const long int kOneAsInteger = AsInteger(1.0f);

inline float inline_quick_sqrt(float x) {
  int i;
  i = (AsInteger(x) >> 1) + (kOneAsInteger >> 1);
  return AsFloat(i);
}

inline float inline_sqrt(float x) {
  float y;
  y = inline_quick_sqrt(x);
  y = (y * y + x) / (2.0f * y);
  y = (y * y + x) / (2.0f * y);
  return y;
}

// takes a -0..1+ color, clamps it to 0..1 and maps it to 0..255 integer
inline uint32_t Clamp255(float x) {
  if (x < 0.0f) {
    x = 0.0f;
  } else if (x > 1.0f) {
    x = 1.0f;
  }
  return static_cast<uint32_t>(x * 255.0f);
}
}  // namespace


// The main object that runs the Earth demo.
class Planet {
 public:
  Planet();
  virtual ~Planet();
  // Runs a tick of the simulations, update 2D output.
  void Update();
  // Handle event from user, or message from JS.
  void HandleEvent(PSEvent* ps_event);

 private:
  // Methods prefixed with 'w' are run on worker threads.
  uint32_t* wGetAddr(int x, int y);
  void wRenderPixelSpan(int x0, int x1, int y);
  void wMakeRect(int r, int *x, int *y, int *w, int *h);
  void wRenderRect(int x0, int y0, int x1, int y1);
  void wRenderRegion(int region);
  static void wRenderRegionEntry(int region, void *thiz);

  // These methods are only called by the main thread.
  void CacheCalcs();
  void SetPlanetXYZR(float x, float y, float z, float r);
  void SetPlanetPole(float x, float y, float z);
  void SetPlanetEquator(float x, float y, float z);
  void SetPlanetSpin(float x, float y);
  void SetEyeXYZ(float x, float y, float z);
  void SetLightXYZ(float x, float y, float z);
  void SetAmbientRGB(float r, float g, float b);
  void SetDiffuseRGB(float r, float g, float b);
  void SetZoom(float zoom);
  void SetLight(float zoom);
  void SetTexture(const std::string& name, int width, int height,
      uint32_t* pixels);
  void SpinPlanet(pp::Point new_point, pp::Point last_point);

  void Reset();
  void RequestTextures();
  void UpdateSim();
  void Render();
  void Draw();
  void StartBenchmark();
  void EndBenchmark();
  // Post a small key-value message to update JS.
  void PostUpdateMessage(const char* message_name, double value);

  // User Interface settings.  These settings are controlled via html
  // controls or via user input.
  float ui_light_;
  float ui_zoom_;
  float ui_spin_x_;
  float ui_spin_y_;
  pp::Point ui_last_point_;

  // Various settings for position & orientation of planet.  Do not change
  // these variables, instead use SetPlanet*() functions.
  float planet_radius_;
  float planet_spin_x_;
  float planet_spin_y_;
  float planet_x_, planet_y_, planet_z_;
  float planet_pole_x_, planet_pole_y_, planet_pole_z_;
  float planet_equator_x_, planet_equator_y_, planet_equator_z_;

  // Observer's eye.  Do not change these variables, instead use SetEyeXYZ().
  float eye_x_, eye_y_, eye_z_;

  // Light position, ambient and diffuse settings.  Do not change these
  // variables, instead use SetLightXYZ(), SetAmbientRGB() and SetDiffuseRGB().
  float light_x_, light_y_, light_z_;
  float diffuse_r_, diffuse_g_, diffuse_b_;
  float ambient_r_, ambient_g_, ambient_b_;

  // Cached calculations.  Do not change these variables - they are updated by
  // CacheCalcs() function.
  float planet_xyz_;
  float planet_pole_x_equator_x_;
  float planet_pole_x_equator_y_;
  float planet_pole_x_equator_z_;
  float planet_radius2_;
  float planet_one_over_radius_;
  float eye_xyz_;

  // Source texture (earth map).
  Texture* base_tex_;
  Texture* night_tex_;
  int width_for_tex_;
  int height_for_tex_;

  // Quick ArcCos helper.
  ArcCosine acos_;

  // Misc.
  PSContext2D_t* ps_context_;
  int num_threads_;
  ThreadPool* workers_;
  bool benchmarking_;
  int benchmark_frame_counter_;
  double benchmark_start_time_;
  double benchmark_end_time_;
};


void Planet::RequestTextures() {
  // Request a set of images from JS.  After images are loaded by JS, a
  // message from JS -> NaCl will arrive containing the pixel data.  See
  // HandleMessage() method in this file.
  pp::VarDictionary message;
  message.Set("message", "request_textures");
  pp::VarArray names;
  names.Set(0, "earth.jpg");
  names.Set(1, "earthnight.jpg");
  message.Set("names", names);
  PSInterfaceMessaging()->PostMessage(PSGetInstanceId(), message.pp_var());
}

void Planet::Reset() {
  // Reset has to first fill in all variables with valid floats, so
  // CacheCalcs() doesn't potentially propagate NaNs when calling Set*()
  // functions further below.
  planet_radius_ = 1.0f;
  planet_spin_x_ = 0.0f;
  planet_spin_y_ = 0.0f;
  planet_x_ = 0.0f;
  planet_y_ = 0.0f;
  planet_z_ = 0.0f;
  planet_pole_x_ = 0.0f;
  planet_pole_y_ = 0.0f;
  planet_pole_z_ = 0.0f;
  planet_equator_x_ = 0.0f;
  planet_equator_y_ = 0.0f;
  planet_equator_z_ = 0.0f;
  eye_x_ = 0.0f;
  eye_y_ = 0.0f;
  eye_z_ = 0.0f;
  light_x_ = 0.0f;
  light_y_ = 0.0f;
  light_z_ = 0.0f;
  diffuse_r_ = 0.0f;
  diffuse_g_ = 0.0f;
  diffuse_b_ = 0.0f;
  ambient_r_ = 0.0f;
  ambient_g_ = 0.0f;
  ambient_b_ = 0.0f;
  planet_xyz_ = 0.0f;
  planet_pole_x_equator_x_ = 0.0f;
  planet_pole_x_equator_y_ = 0.0f;
  planet_pole_x_equator_z_ = 0.0f;
  planet_radius2_ = 0.0f;
  planet_one_over_radius_ = 0.0f;
  eye_xyz_ = 0.0f;
  ui_zoom_ = 14.0f;
  ui_light_ = 1.0f;
  ui_spin_x_ = 0.01f;
  ui_spin_y_ = 0.0f;
  ui_last_point_ = pp::Point(0, 0);

  // Set up reasonable default values.
  SetPlanetXYZR(0.0f, 0.0f, 48.0f, 4.0f);
  SetEyeXYZ(0.0f, 0.0f, -ui_zoom_);
  SetLightXYZ(-60.0f, -30.0f, 0.0f);
  SetAmbientRGB(0.05f, 0.05f, 0.05f);
  SetDiffuseRGB(0.8f, 0.8f, 0.8f);
  SetPlanetPole(0.0f, 1.0f, 0.0f);
  SetPlanetEquator(1.0f, 0.0f, 0.0f);
  SetPlanetSpin(kPI / 2.0f, kPI / 2.0f);
  SetZoom(ui_zoom_);
  SetLight(ui_light_);

  // Send UI values to JS to reset html sliders.
  PostUpdateMessage("set_zoom", ui_zoom_);
  PostUpdateMessage("set_light", ui_light_);
}


Planet::Planet() : base_tex_(NULL), night_tex_(NULL), num_threads_(0),
    benchmarking_(false), benchmark_frame_counter_(0) {

  Reset();
  RequestTextures();
  // By default, render from the dispatch thread.
  workers_ = new ThreadPool(num_threads_);
  PSEventSetFilter(PSE_ALL);
  ps_context_ = PSContext2DAllocate(PP_IMAGEDATAFORMAT_BGRA_PREMUL);
}

Planet::~Planet() {
  delete workers_;
  PSContext2DFree(ps_context_);
}

// Given a region r, derive a rectangle.
// This rectangle shouldn't overlap with work being done by other workers.
// If multithreading, this function is only called by the worker threads.
void Planet::wMakeRect(int r, int *x, int *y, int *w, int *h) {
  *x = 0;
  *w = ps_context_->width;
  *y = r;
  *h = 1;
}


inline uint32_t* Planet::wGetAddr(int x, int y) {
  return ps_context_->data + x + y * ps_context_->stride / sizeof(uint32_t);
}

// This is the meat of the ray tracer.  Given a pixel span (x0, x1) on
// scanline y, shoot rays into the scene and render what they hit.  Use
// scanline coherence to do a few optimizations
void Planet::wRenderPixelSpan(int x0, int x1, int y) {
  if (!base_tex_ || !night_tex_)
    return;
  const int kColorBlack = MakeBGRA(0, 0, 0, 0xFF);
  float width = ps_context_->width;
  float height = ps_context_->height;
  float min_dim = width < height ? width : height;
  float offset_x = width < height ? 0 : (width - min_dim) * 0.5f;
  float offset_y = width < height ? (height - min_dim) * 0.5f : 0;
  float y0 = eye_y_;
  float z0 = eye_z_;
  float y1 = (static_cast<float>(y - offset_y) / min_dim) * 2.0f - 1.0f;
  float z1 = 0.0f;
  float dy = (y1 - y0);
  float dz = (z1 - z0);
  float dy_dy_dz_dz = dy * dy + dz * dz;
  float two_dy_y0_y_two_dz_z0_z = 2.0f * dy * (y0 - planet_y_) +
                                  2.0f * dz * (z0 - planet_z_);
  float planet_xyz_eye_xyz = planet_xyz_ + eye_xyz_;
  float y_y0_z_z0 = planet_y_ * y0 + planet_z_ * z0;
  float oowidth = 1.0f / min_dim;
  uint32_t* pixels = this->wGetAddr(x0, y);
  for (int x = x0; x <= x1; ++x) {
    // scan normalized screen -1..1
    float x1 = (static_cast<float>(x - offset_x) * oowidth) * 2.0f - 1.0f;
    // eye
    float x0 = eye_x_;
    // delta from screen to eye
    float dx = (x1 - x0);
    // build a, b, c
    float a = dx * dx + dy_dy_dz_dz;
    float b = 2.0f * dx * (x0 - planet_x_) + two_dy_y0_y_two_dz_z0_z;
    float c = planet_xyz_eye_xyz +
              -2.0f * (planet_x_ * x0 + y_y0_z_z0) - (planet_radius2_);
    // calculate discriminant
    float disc = b * b - 4.0f * a * c;

    // Did ray hit the sphere?
    if (disc < 0.0f) {
      *pixels = kColorBlack;
      ++pixels;
      continue;
    }

    // calc parametric t value
    float t = (-b - inline_sqrt(disc)) / (2.0f * a);
    float px = x0 + t * dx;
    float py = y0 + t * dy;
    float pz = z0 + t * dz;
    float nx = (px - planet_x_) * planet_one_over_radius_;
    float ny = (py - planet_y_) * planet_one_over_radius_;
    float nz = (pz - planet_z_) * planet_one_over_radius_;

    // Misc raytrace calculations.
    float Lx = (light_x_ - px);
    float Ly = (light_y_ - py);
    float Lz = (light_z_ - pz);
    float Lq = 1.0f / inline_quick_sqrt(Lx * Lx + Ly * Ly + Lz * Lz);
    Lx *= Lq;
    Ly *= Lq;
    Lz *= Lq;
    float d = (Lx * nx + Ly * ny + Lz * nz);
    float pr = (diffuse_r_ * d) + ambient_r_;
    float pg = (diffuse_g_ * d) + ambient_g_;
    float pb = (diffuse_b_ * d) + ambient_b_;
    float ds = -(nx * planet_pole_x_ +
                 ny * planet_pole_y_ +
                 nz * planet_pole_z_);
    float ang = acos_.TableLerp(ds);
    float v = ang * kOneOverPI;
    float dp = planet_equator_x_ * nx +
               planet_equator_y_ * ny +
               planet_equator_z_ * nz;
    float w = dp / sinf(ang);
    if (w > 1.0f) w = 1.0f;
    if (w < -1.0f) w = -1.0f;
    float th = acos_.TableLerp(w) * kOneOver2PI;
    float dps = planet_pole_x_equator_x_ * nx +
                planet_pole_x_equator_y_ * ny +
                planet_pole_x_equator_z_ * nz;
    float u;
    if (dps < 0.0f)
      u = th;
    else
      u = 1.0f - th;

    // Look up daylight texel.
    int tx = static_cast<int>(u * base_tex_->width);
    int ty = static_cast<int>(v * base_tex_->height);
    int offset = tx + ty * base_tex_->width;
    uint32_t base_texel = base_tex_->pixels[offset];
    float tr = ExtractR(base_texel);
    float tg = ExtractG(base_texel);
    float tb = ExtractB(base_texel);

    float ipr = 1.0f - pr;
    if (ipr < 0.0f) ipr = 0.0f;
    float ipg = 1.0f - pg;
    if (ipg < 0.0f) ipg = 0.0f;
    float ipb = 1.0f - pb;
    if (ipb < 0.0f) ipb = 0.0f;

    // Look up night texel.
    int nix = static_cast<int>(u * night_tex_->width);
    int niy = static_cast<int>(v * night_tex_->height);
    int noffset = nix + niy * night_tex_->width;
    uint32_t night_texel = night_tex_->pixels[noffset];
    float nr = ExtractR(night_texel);
    float ng = ExtractG(night_texel);
    float nb = ExtractB(night_texel);

    // Final color value is lerp between day and night texels.
    unsigned int ir = Clamp255(pr * tr + nr * ipr);
    unsigned int ig = Clamp255(pg * tg + ng * ipg);
    unsigned int ib = Clamp255(pb * tb + nb * ipb);

    unsigned int color = MakeBGRA(ib, ig, ir, 0xFF);

    *pixels = color;
    ++pixels;
  }
}

// Renders a rectangular area of the screen, scan line at a time
void Planet::wRenderRect(int x, int y, int w, int h) {
  for (int j = y; j < (y + h); ++j) {
    this->wRenderPixelSpan(x, x + w - 1, j);
  }
}

// If multithreading, this function is only called by the worker threads.
void Planet::wRenderRegion(int region) {
  // convert region # into x0, y0, x1, y1 rectangle
  int x, y, w, h;
  wMakeRect(region, &x, &y, &w, &h);
  // render this rectangle
  wRenderRect(x, y, w, h);
}

// Entry point for worker thread.  Can't pass a member function around, so we
// have to do this little round-about.
void Planet::wRenderRegionEntry(int region, void* thiz) {
  static_cast<Planet*>(thiz)->wRenderRegion(region);
}

// Renders the planet, dispatching the work to multiple threads.
void Planet::Render() {
  workers_->Dispatch(ps_context_->height, wRenderRegionEntry, this);
}

// Pre-calculations to make inner loops faster.
void Planet::CacheCalcs() {
  planet_xyz_ = planet_x_ * planet_x_ +
                planet_y_ * planet_y_ +
                planet_z_ * planet_z_;
  planet_radius2_ = planet_radius_ * planet_radius_;
  planet_one_over_radius_ = 1.0f / planet_radius_;
  eye_xyz_ = eye_x_ * eye_x_ + eye_y_ * eye_y_ + eye_z_ * eye_z_;
  // spin vector from center->equator
  planet_equator_x_ = cos(planet_spin_x_);
  planet_equator_y_ = 0.0f;
  planet_equator_z_ = sin(planet_spin_x_);

  // cache cross product of pole & equator
  planet_pole_x_equator_x_ = planet_pole_y_ * planet_equator_z_ -
                             planet_pole_z_ * planet_equator_y_;
  planet_pole_x_equator_y_ = planet_pole_z_ * planet_equator_x_ -
                             planet_pole_x_ * planet_equator_z_;
  planet_pole_x_equator_z_ = planet_pole_x_ * planet_equator_y_ -
                             planet_pole_y_ * planet_equator_x_;
}

void Planet::SetPlanetXYZR(float x, float y, float z, float r) {
  planet_x_ = x;
  planet_y_ = y;
  planet_z_ = z;
  planet_radius_ = r;
  CacheCalcs();
}

void Planet::SetEyeXYZ(float x, float y, float z) {
  eye_x_ = x;
  eye_y_ = y;
  eye_z_ = z;
  CacheCalcs();
}

void Planet::SetLightXYZ(float x, float y, float z) {
  light_x_ = x;
  light_y_ = y;
  light_z_ = z;
  CacheCalcs();
}

void Planet::SetAmbientRGB(float r, float g, float b) {
  ambient_r_ = r;
  ambient_g_ = g;
  ambient_b_ = b;
  CacheCalcs();
}

void Planet::SetDiffuseRGB(float r, float g, float b) {
  diffuse_r_ = r;
  diffuse_g_ = g;
  diffuse_b_ = b;
  CacheCalcs();
}

void Planet::SetPlanetPole(float x, float y, float z) {
  planet_pole_x_ = x;
  planet_pole_y_ = y;
  planet_pole_z_ = z;
  CacheCalcs();
}

void Planet::SetPlanetEquator(float x, float y, float z) {
  // This is really over-ridden by spin at the momenent.
  planet_equator_x_ = x;
  planet_equator_y_ = y;
  planet_equator_z_ = z;
  CacheCalcs();
}

void Planet::SetPlanetSpin(float x, float y) {
  planet_spin_x_ = x;
  planet_spin_y_ = y;
  CacheCalcs();
}

// Run a simple sim to spin the planet.  Update loop is run once per frame.
// Called from the main thread only and only when the worker threads are idle.
void Planet::UpdateSim() {
  float x = planet_spin_x_ + ui_spin_x_;
  float y = planet_spin_y_ + ui_spin_y_;
  // keep in nice range
  if (x > (kPI * 2.0f))
    x = x - kPI * 2.0f;
  else if (x < (-kPI * 2.0f))
    x = x + kPI * 2.0f;
  if (y > (kPI * 2.0f))
    y = y - kPI * 2.0f;
  else if (y < (-kPI * 2.0f))
    y = y + kPI * 2.0f;
  SetPlanetSpin(x, y);
}

void Planet::StartBenchmark() {
  // For more consistent benchmark numbers, reset to default state.
  Reset();
  printf("Benchmark started...\n");
  benchmark_frame_counter_ = kFramesToBenchmark;
  benchmarking_ = true;
  benchmark_start_time_ = getseconds();
}

void Planet::EndBenchmark() {
  benchmark_end_time_ = getseconds();
  printf("Benchmark ended... time: %2.5f\n",
      benchmark_end_time_ - benchmark_start_time_);
  benchmarking_ = false;
  benchmark_frame_counter_ = 0;
  double total_time = benchmark_end_time_ - benchmark_start_time_;
  // Send benchmark result to JS.
  PostUpdateMessage("benchmark_result", total_time);
}

void Planet::SetZoom(float zoom) {
  ui_zoom_ = std::min(kZoomMax, std::max(kZoomMin, zoom));
  SetEyeXYZ(0.0f, 0.0f, -ui_zoom_);
}

void Planet::SetLight(float light) {
  ui_light_ = std::min(kLightMax, std::max(kLightMin, light));
  SetDiffuseRGB(0.8f * ui_light_, 0.8f * ui_light_, 0.8f * ui_light_);
  SetAmbientRGB(0.4f * ui_light_, 0.4f * ui_light_, 0.4f * ui_light_);
}

void Planet::SetTexture(const std::string& name, int width, int height,
                        uint32_t* pixels) {
  if (pixels) {
    if (name == "earth.jpg") {
      delete base_tex_;
      base_tex_ = new Texture(width, height, pixels);
    } else if (name == "earthnight.jpg") {
      delete night_tex_;
      night_tex_ = new Texture(width, height, pixels);
    }
  }
}

void Planet::SpinPlanet(pp::Point new_point, pp::Point last_point) {
  float delta_x = static_cast<float>(new_point.x() - last_point.x());
  float delta_y = static_cast<float>(new_point.y() - last_point.y());
  float spin_x = std::min(10.0f, std::max(-10.0f, delta_x * 0.5f));
  float spin_y = std::min(10.0f, std::max(-10.0f, delta_y * 0.5f));
  ui_spin_x_ = spin_x / 100.0f;
  ui_spin_y_ = spin_y / 100.0f;
  ui_last_point_ = new_point;
}

// Handle input events from the user and messages from JS.
void Planet::HandleEvent(PSEvent* ps_event) {
  // Give the 2D context a chance to process the event.
  if (0 != PSContext2DHandleEvent(ps_context_, ps_event))
    return;
  if (ps_event->type == PSE_INSTANCE_HANDLEINPUT) {
    // Convert Pepper Simple event to a PPAPI C++ event
    pp::InputEvent event(ps_event->as_resource);
    switch (event.GetType()) {
      case PP_INPUTEVENT_TYPE_KEYDOWN: {
        pp::KeyboardInputEvent key(event);
        uint32_t key_code = key.GetKeyCode();
        if (key_code == 84)  // 't' key
          if (!benchmarking_)
            StartBenchmark();
        break;
      }
      case PP_INPUTEVENT_TYPE_MOUSEDOWN:
      case PP_INPUTEVENT_TYPE_MOUSEMOVE: {
        pp::MouseInputEvent mouse = pp::MouseInputEvent(event);
        if (mouse.GetModifiers() & PP_INPUTEVENT_MODIFIER_LEFTBUTTONDOWN) {
          if (event.GetType() == PP_INPUTEVENT_TYPE_MOUSEDOWN)
            SpinPlanet(mouse.GetPosition(), mouse.GetPosition());
          else
            SpinPlanet(mouse.GetPosition(), ui_last_point_);
        }
        break;
      }
      case PP_INPUTEVENT_TYPE_WHEEL: {
        pp::WheelInputEvent wheel = pp::WheelInputEvent(event);
        PP_FloatPoint ticks = wheel.GetTicks();
        SetZoom(ui_zoom_ + (ticks.x + ticks.y) * kWheelSpeed);
        // Update html slider by sending update message to JS.
        PostUpdateMessage("set_zoom", ui_zoom_);
        break;
      }
      case PP_INPUTEVENT_TYPE_TOUCHSTART:
      case PP_INPUTEVENT_TYPE_TOUCHMOVE: {
        pp::TouchInputEvent touches = pp::TouchInputEvent(event);
        uint32_t count = touches.GetTouchCount(PP_TOUCHLIST_TYPE_TOUCHES);
        if (count > 0) {
          // Use first touch point to spin planet.
          pp::TouchPoint touch =
              touches.GetTouchByIndex(PP_TOUCHLIST_TYPE_TOUCHES, 0);
          pp::Point screen_point(touch.position().x(),
                                 touch.position().y());
          if (event.GetType() == PP_INPUTEVENT_TYPE_TOUCHSTART)
            SpinPlanet(screen_point, screen_point);
          else
            SpinPlanet(screen_point, ui_last_point_);
        }
        break;
      }
      default:
        break;
    }
  } else if (ps_event->type == PSE_INSTANCE_HANDLEMESSAGE) {
    // Convert Pepper Simple message to PPAPI C++ vars
    pp::Var var(ps_event->as_var);
    if (var.is_dictionary()) {
      pp::VarDictionary dictionary(var);
      std::string message = dictionary.Get("message").AsString();
      if (message == "run benchmark" && !benchmarking_) {
        StartBenchmark();
      } else if (message == "set_light") {
        SetLight(static_cast<float>(dictionary.Get("value").AsDouble()));
      } else if (message == "set_zoom") {
        SetZoom(static_cast<float>(dictionary.Get("value").AsDouble()));
      } else if (message == "set_threads") {
        int threads = dictionary.Get("value").AsInt();
        delete workers_;
        workers_ = new ThreadPool(threads);
      } else if (message == "texture") {
        std::string name = dictionary.Get("name").AsString();
        int width = dictionary.Get("width").AsInt();
        int height = dictionary.Get("height").AsInt();
        pp::VarArrayBuffer array_buffer(dictionary.Get("data"));
        if (!name.empty() && !array_buffer.is_null()) {
          if (width > 0 && height > 0) {
            uint32_t* pixels = static_cast<uint32_t*>(array_buffer.Map());
            SetTexture(name, width, height, pixels);
            array_buffer.Unmap();
          }
        }
      }
    } else {
      printf("Handle message unknown type: %s\n", var.DebugString().c_str());
    }
  }
}

// PostUpdateMessage() helper function for sending small messages to JS.
void Planet::PostUpdateMessage(const char* message_name, double value) {
  pp::VarDictionary message;
  message.Set("message", message_name);
  message.Set("value", value);
  PSInterfaceMessaging()->PostMessage(PSGetInstanceId(), message.pp_var());
}

void Planet::Update() {
  // When benchmarking is running, don't update display via
  // PSContext2DSwapBuffer() - vsync is enabled by default, and will throttle
  // the benchmark results.
  PSContext2DGetBuffer(ps_context_);
  if (NULL == ps_context_->data)
    return;

  do {
    UpdateSim();
    Render();
    if (!benchmarking_) break;
    --benchmark_frame_counter_;
  } while (benchmark_frame_counter_ > 0);
  if (benchmarking_)
    EndBenchmark();

  PSContext2DSwapBuffer(ps_context_);
}


// Starting point for the module.
int main(int argc, char* argv[]) {
  Planet earth;
  while (true) {
    PSEvent* ps_event;
    // Consume all available events
    while ((ps_event = PSEventTryAcquire()) != NULL) {
      earth.HandleEvent(ps_event);
      PSEventRelease(ps_event);
    }
    // Do simulation, render and present.
    earth.Update();
  }

  return 0;
}
